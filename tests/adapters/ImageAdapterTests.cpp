// Issue #1412（親 #1381・施主決定 D79・ADR 0155 の追補 54）:
// プレビューの画像の読み（Win32ImageAdapter）を、本物のファイル・
// 本物のワーカー 1 本・本物の WIC で測る。見本の PNG / JPEG / BMP は
// この試験が WIC の符号器で作り、GIF（2 コマ）と大きな 1 bpp の BMP は
// バイト列を手で組む。一時フォルダは temporary_test_directory（#1103）の中に
// 作り、終わりに消す。時刻は読まない（Sleep・時計・タイムアウトの待ちを
// 書かない）。待つのは合図の受け皿（セマフォ）だけで、止まらないときは
// CTest の制限時間で落ちる。
#include <initguid.h>

#include <wincodec.h>

#include "FilePath.hpp"
#include "ImageFailure.hpp"
#include "ImageGeometry.hpp"
#include "ImageLimits.hpp"
#include "ImageOutcome.hpp"
#include "ImagePixels.hpp"
#include "ImageRequest.hpp"
#include "ImageSize.hpp"
#include "TemporaryTestDirectory.hpp"
#include "Win32ImageAdapter.hpp"
#include "Win32Worker.hpp"

#include <objbase.h>
#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <semaphore>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
namespace adapters = nenenib::adapters::win32;
using Microsoft::WRL::ComPtr;
using Arrivals = std::counting_semaphore<>;
using Outcomes = std::vector<app::ImageOutcome>;
using Color = std::array<std::uint8_t, 4>; // B G R A（alpha を掛けていない）

constexpr wchar_t root[] = L"nib-image-files";

std::size_t &failures()
{
    static std::size_t count = 0;
    return count;
}

std::size_t &checks()
{
    static std::size_t count = 0;
    return count;
}

void expect(bool condition, const char *message)
{
    ++checks();
    if (!condition)
    {
        ++failures();
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}

[[nodiscard]] std::wstring inside(std::wstring_view name)
{
    return std::wstring(root) + L"\\" + std::wstring(name);
}

[[nodiscard]] core::FilePath path_of(std::string_view name)
{
    return core::FilePath::parse(std::format("nib-image-files\\{}", name)).value();
}

void write_file(std::wstring_view name, std::string_view bytes)
{
    const std::wstring path = inside(name);
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD written = 0;
    const bool wrote =
        file != INVALID_HANDLE_VALUE &&
        (bytes.empty() ||
         WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != 0) &&
        written == bytes.size();
    expect(wrote, "a sample file is written");
    if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
    }
}

// stream の中身を最初から全部読む。
[[nodiscard]] std::string stream_bytes(IStream &stream)
{
    STATSTG stat{};
    const LARGE_INTEGER origin{};
    if (FAILED(stream.Stat(&stat, STATFLAG_NONAME)) ||
        FAILED(stream.Seek(origin, STREAM_SEEK_SET, nullptr)))
    {
        return {};
    }
    std::string bytes(static_cast<std::size_t>(stat.cbSize.QuadPart), '\0');
    ULONG read = 0;
    if (FAILED(stream.Read(bytes.data(), static_cast<ULONG>(bytes.size()), &read)) ||
        read != bytes.size())
    {
        return {};
    }
    return bytes;
}

// 1 色で塗った width × height の画像を container の WIC の符号器で書いたバイト列（失敗は空）。
[[nodiscard]] std::string encoded(const GUID &container, UINT width, UINT height, Color color)
{
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmap> bitmap;
    ComPtr<IStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    std::vector<BYTE> pixels(std::size_t{width} * height * 4U);
    for (std::size_t index = 0; index < pixels.size(); ++index)
    {
        pixels.at(index) = color.at(index % 4U);
    }
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    const bool made =
        SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory))) &&
        SUCCEEDED(factory->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppBGRA,
                                                  width * 4U, static_cast<UINT>(pixels.size()),
                                                  pixels.data(), &bitmap)) &&
        SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) &&
        SUCCEEDED(factory->CreateEncoder(container, nullptr, &encoder)) &&
        SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) &&
        SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) &&
        SUCCEEDED(frame->Initialize(nullptr)) && SUCCEEDED(frame->SetSize(width, height)) &&
        SUCCEEDED(frame->SetPixelFormat(&format)) &&
        SUCCEEDED(frame->WriteSource(bitmap.Get(), nullptr)) && SUCCEEDED(frame->Commit()) &&
        SUCCEEDED(encoder->Commit());
    return made ? stream_bytes(*stream.Get()) : std::string{};
}

void append_u16(std::string &bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<char>(value & 0xFFU));
    bytes.push_back(static_cast<char>((value >> 8U) & 0xFFU));
}

void append_u32(std::string &bytes, std::uint32_t value)
{
    append_u16(bytes, value & 0xFFFFU);
    append_u16(bytes, value >> 16U);
}

// 1 × 1 の 2 コマの GIF。色の表は 0 = 赤・1 = 青で、1 コマ目は 0、
// 2 コマ目は 1 の画素（LZW の最小の符号の長さ 2 で
// 「消す 4・画素・終わり 5」の 3 bit の符号）。
[[nodiscard]] std::string two_frame_gif()
{
    std::string bytes = "GIF89a";
    append_u16(bytes, 1U);
    append_u16(bytes, 1U);
    bytes += std::string("\x80\x00\x00", 3);             // 全体の色の表あり・2 色・背景 0・縦横比 0
    bytes += std::string("\xFF\x00\x00\x00\x00\xFF", 6); // 赤・青
    const std::array<std::string_view, 2> pixel_data{std::string_view("\x02\x02\x44\x01\x00", 5),
                                                     std::string_view("\x02\x02\x4C\x01\x00", 5)};
    for (const std::string_view data : pixel_data)
    {
        bytes += std::string("\x2C\x00\x00\x00\x00\x01\x00\x01\x00\x00", 10);
        bytes += data;
    }
    bytes += ';';
    return bytes;
}

// 正しい形の 1 bpp の BMP（width × height・下から上・画素は全部 0）。
[[nodiscard]] std::string one_bit_bmp(std::uint32_t width, std::uint32_t height)
{
    const std::uint32_t row = ((width + 31U) / 32U) * 4U;
    const std::uint32_t offset = 14U + 40U + 8U;
    std::string bytes = "BM";
    append_u32(bytes, offset + (row * height));
    append_u32(bytes, 0U);
    append_u32(bytes, offset);
    append_u32(bytes, 40U);
    append_u32(bytes, width);
    append_u32(bytes, height);
    append_u16(bytes, 1U);
    append_u16(bytes, 1U);
    append_u32(bytes, 0U);
    append_u32(bytes, row * height);
    append_u32(bytes, 2835U);
    append_u32(bytes, 2835U);
    append_u32(bytes, 2U);
    append_u32(bytes, 0U);
    append_u32(bytes, 0x00000000U);
    append_u32(bytes, 0x00FFFFFFU);
    bytes.resize(bytes.size() + (std::size_t{row} * height), '\0');
    return bytes;
}

// requests を 1 つの adapter に全部頼み、合図を待っては受け取って、
// 要求の数だけ結果が来るまで続ける。最後の結果の後にほかの結果が
// 残っていないことも見る（要求ごとにちょうど 1 つ）。
[[nodiscard]] Outcomes read_all(const std::vector<app::ImageRequest> &requests)
{
    Arrivals arrived{0};
    adapters::Win32Worker worker;
    adapters::Win32ImageAdapter images(worker);
    images.bind([&arrived] { arrived.release(); });
    for (const auto &request : requests)
    {
        images.request(request);
    }
    Outcomes outcomes;
    while (outcomes.size() < requests.size())
    {
        arrived.acquire();
        for (auto &outcome : images.collect())
        {
            outcomes.push_back(std::move(outcome));
        }
    }
    expect(images.collect().empty(), "nothing is left after the result of every request");
    return outcomes;
}

[[nodiscard]] app::ImageOutcome read_one(std::string_view name)
{
    auto outcomes = read_all({app::ImageRequest{1U, path_of(name)}});
    return std::move(outcomes.front());
}

[[nodiscard]] bool failed_with(const app::ImageOutcome &outcome, app::ImageFailure failure)
{
    return outcome.ticket == 1U && !outcome.result.has_value() && outcome.result.error() == failure;
}

[[nodiscard]] const app::ImagePixels *pixels_of(const app::ImageOutcome &outcome)
{
    return outcome.result.has_value() ? outcome.result.value().get() : nullptr;
}

// 画素が全部 expected（premultiplied）から tolerance 以内か。
[[nodiscard]] bool filled_with(const app::ImagePixels &pixels, Color expected, int tolerance)
{
    for (std::size_t index = 0; index < pixels.bgra.size(); ++index)
    {
        const int difference = int{pixels.bgra.at(index)} - int{expected.at(index % 4U)};
        if (difference > tolerance || difference < -tolerance)
        {
            return false;
        }
    }
    return true;
}

// 決まり（追補 53）: size は image_decoded_size(original)・bgra は詰め物なしの size の大きさ。
[[nodiscard]] bool well_formed(const app::ImagePixels &pixels, core::ImageSize original)
{
    return pixels.original == original && pixels.size == core::image_decoded_size(original) &&
           pixels.bgra.size() == std::size_t{pixels.size.width} * pixels.size.height * 4U;
}

void verify_not_loaded_before_a_request()
{
    expect(GetModuleHandleW(L"windowscodecs.dll") == nullptr,
           "the test process starts without windowscodecs.dll");
    adapters::Win32Worker worker;
    {
        adapters::Win32ImageAdapter images(worker);
        images.bind([] {});
        expect(images.collect().empty(), "nothing is collected before a request");
    }
    expect(!worker.started() && GetModuleHandleW(L"windowscodecs.dll") == nullptr,
           "making and binding the adapter starts no thread and loads no WIC");
}

// 知らない形式・無いファイルは COM に触れずに返る（V-1426 の軽い 2）。WIC を読む見本の
// 符号器より前に頼み、windowscodecs.dll がまだ process に無いことを見る。
void verify_unknown_needs_no_wic()
{
    write_file(L"early.png", "plain text, not an image");
    expect(failed_with(read_one("early.png"), app::ImageFailure::unsupported) &&
               failed_with(read_one("early-absent.png"), app::ImageFailure::missing),
           "an unknown format and a missing file are answered");
    expect(GetModuleHandleW(L"windowscodecs.dll") == nullptr,
           "an unknown format and a missing file load no WIC");
}

void verify_formats()
{
    const Color opaque{10U, 20U, 30U, 255U};
    write_file(L"a.png", encoded(GUID_ContainerFormatPng, 3U, 2U, opaque));
    write_file(L"a.bmp", encoded(GUID_ContainerFormatBmp, 3U, 2U, opaque));
    write_file(L"a.jpg", encoded(GUID_ContainerFormatJpeg, 16U, 8U, Color{40U, 80U, 160U, 255U}));
    write_file(L"two.gif", two_frame_gif());
    const auto png = read_one("a.png");
    expect(pixels_of(png) != nullptr && well_formed(*pixels_of(png), {3U, 2U}) &&
               filled_with(*pixels_of(png), opaque, 0),
           "a PNG is decoded to 32bppPBGRA rows without padding");
    const auto bmp = read_one("a.bmp");
    expect(pixels_of(bmp) != nullptr && well_formed(*pixels_of(bmp), {3U, 2U}) &&
               filled_with(*pixels_of(bmp), opaque, 0),
           "a BMP is decoded");
    const auto jpeg = read_one("a.jpg");
    expect(pixels_of(jpeg) != nullptr && well_formed(*pixels_of(jpeg), {16U, 8U}) &&
               filled_with(*pixels_of(jpeg), Color{40U, 80U, 160U, 255U}, 8),
           "a JPEG is decoded (the colour within the loss of the encoding)");
    const auto gif = read_one("two.gif");
    expect(pixels_of(gif) != nullptr && well_formed(*pixels_of(gif), {1U, 1U}) &&
               filled_with(*pixels_of(gif), Color{0U, 0U, 255U, 255U}, 0),
           "a GIF of two frames shows the first frame (red, not blue)");
}

void verify_premultiplied_and_scaled()
{
    write_file(L"half.png", encoded(GUID_ContainerFormatPng, 2U, 2U, Color{0U, 0U, 200U, 128U}));
    const auto half = read_one("half.png");
    expect(pixels_of(half) != nullptr &&
               filled_with(*pixels_of(half), Color{0U, 0U, 100U, 128U}, 1),
           "the colour of a half transparent pixel is multiplied by its alpha");
    const Color solid{50U, 100U, 150U, 255U};
    write_file(L"wide.png", encoded(GUID_ContainerFormatPng, 4100U, 6U, solid));
    const auto wide = read_one("wide.png");
    expect(pixels_of(wide) != nullptr && well_formed(*pixels_of(wide), {4100U, 6U}) &&
               pixels_of(wide)->size.width == core::image_decoded_side_limit &&
               filled_with(*pixels_of(wide), solid, 1),
           "an image longer than 2048 is shrunk to the decoded size and keeps its original size");
}

void verify_content_decides_the_format()
{
    const Color opaque{1U, 2U, 3U, 255U};
    write_file(L"photo.jpg", encoded(GUID_ContainerFormatPng, 2U, 1U, opaque));
    const auto named_jpeg = read_one("photo.jpg");
    expect(pixels_of(named_jpeg) != nullptr && filled_with(*pixels_of(named_jpeg), opaque, 0),
           "a PNG named .jpg is decoded as a PNG");
    write_file(L"text.png", "not an image at all");
    expect(failed_with(read_one("text.png"), app::ImageFailure::unsupported),
           "a text file named .png is unsupported");
    write_file(L"empty.png", "");
    expect(failed_with(read_one("empty.png"), app::ImageFailure::unsupported),
           "an empty file is unsupported");
    write_file(L"image.webp", std::string("RIFF\x10\x00\x00\x00WEBPVP8 ", 16));
    expect(failed_with(read_one("image.webp"), app::ImageFailure::unsupported),
           "a WebP is unsupported");
    const Color jpeg_colour{40U, 80U, 160U, 255U};
    write_file(L"photo.png", encoded(GUID_ContainerFormatJpeg, 8U, 8U, jpeg_colour));
    const auto named_png = read_one("photo.png");
    expect(pixels_of(named_png) != nullptr && filled_with(*pixels_of(named_png), jpeg_colour, 8),
           "a JPEG named .png is decoded as a JPEG");
    write_file(L"bitmap.gif", encoded(GUID_ContainerFormatBmp, 2U, 2U, opaque));
    const auto named_gif = read_one("bitmap.gif");
    expect(pixels_of(named_gif) != nullptr && filled_with(*pixels_of(named_gif), opaque, 0),
           "a BMP named .gif is decoded as a BMP");
}

void verify_broken_files()
{
    write_file(L"garbage.png", std::string("\x89PNG\r\n\x1A\n", 8) + std::string(64, '\x7F'));
    expect(failed_with(read_one("garbage.png"), app::ImageFailure::corrupt),
           "a PNG signature followed by garbage is corrupt");
    const std::string png = encoded(GUID_ContainerFormatPng, 64U, 64U, Color{9U, 9U, 9U, 255U});
    // 署名と IHDR（33 バイト）だけで画素の塊が無い PNG。
    // IDAT の途中で切れた PNG は WIC が足りない行を埋めて復号に成功する
    // （この機械で 223 バイトの半分で確かめた）ので、壊れた見本にしない。
    write_file(L"cut.png", std::string_view(png).substr(0, 33U));
    expect(failed_with(read_one("cut.png"), app::ImageFailure::corrupt),
           "a PNG cut before its pixel data is corrupt");
    const std::string jpeg = encoded(GUID_ContainerFormatJpeg, 64U, 64U, Color{9U, 9U, 9U, 255U});
    write_file(L"cut.jpg", std::string_view(jpeg).substr(0, 40U));
    expect(failed_with(read_one("cut.jpg"), app::ImageFailure::corrupt), "a cut JPEG is corrupt");
    // 幅か高さが 0 の画像（正しい形の頭）は画素が無いので corrupt。
    write_file(L"zero-wide.bmp", one_bit_bmp(0U, 4U));
    expect(failed_with(read_one("zero-wide.bmp"), app::ImageFailure::corrupt),
           "an image 0 pixels wide is corrupt");
    write_file(L"zero-high.bmp", one_bit_bmp(4U, 0U));
    expect(failed_with(read_one("zero-high.bmp"), app::ImageFailure::corrupt),
           "an image 0 pixels high is corrupt");
}

void verify_limits_and_missing()
{
    // 16 MiB ちょうどは読んで形式の検査へ進み（中身は署名と 0 なので corrupt）、1 バイト超は
    // 読まずに too_large。見本の列は 1 回だけ確保する。
    std::string large;
    large.reserve(core::image_file_byte_limit + 1U);
    large.append("\x89PNG\r\n\x1A\n", 8);
    large.resize(core::image_file_byte_limit, '\0');
    write_file(L"exact.png", large);
    expect(failed_with(read_one("exact.png"), app::ImageFailure::corrupt),
           "a file of exactly 16 MiB is read (corrupt, not too large)");
    large.push_back('\0');
    write_file(L"large.png", large);
    expect(failed_with(read_one("large.png"), app::ImageFailure::too_large),
           "a file over 16 MiB is too large without decoding");
    write_file(L"pixels.bmp", one_bit_bmp(8000U, 8001U));
    expect(failed_with(read_one("pixels.bmp"), app::ImageFailure::too_large),
           "an image over 64,000,000 pixels is too large without decoding");
    expect(failed_with(read_one("absent.png"), app::ImageFailure::missing),
           "a file that is not there is missing");
    expect(CreateDirectoryW(inside(L"folder.png").c_str(), nullptr) != 0, "a folder is made");
    expect(failed_with(read_one("folder.png"), app::ImageFailure::missing),
           "a folder is not an image file (missing)");
    expect(failed_with(read_one("NUL"), app::ImageFailure::unreadable),
           "a reserved device name is not a disk file (unreadable)");
}

// 共有なしで開いたままのファイルは共有違反で開けず unreadable（ほかの道具が書いている途中など）。
void verify_locked_file()
{
    write_file(L"locked.png", encoded(GUID_ContainerFormatPng, 2U, 2U, Color{5U, 6U, 7U, 255U}));
    const std::wstring path = inside(L"locked.png");
    const HANDLE held = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    expect(held != INVALID_HANDLE_VALUE, "the sample is held without sharing");
    expect(failed_with(read_one("locked.png"), app::ImageFailure::unreadable),
           "a file held without sharing is unreadable");
    if (held != INVALID_HANDLE_VALUE)
    {
        CloseHandle(held);
    }
    expect(read_one("locked.png").result.has_value(), "the same file is read once it is released");
}

// 要求ごとに結果がちょうど 1 つ（追補 54・V-1385b の推奨 1）。
// 読めるものと読めないものを混ぜて一度に頼み、届いた順は要求の順。同じワーカーで 1 枚ごとに
// MTA を作って壊し、次の 1 枚で作り直す道も通る（試験のスレッドは製品と同じ STA）。
void verify_one_result_each()
{
    std::vector<app::ImageRequest> requests;
    const std::array<std::string_view, 6> names{"a.png",       "absent.png", "text.png",
                                                "garbage.png", "folder.png", "two.gif"};
    std::uint64_t ticket = 0;
    for (std::size_t round = 0; round < 3U; ++round)
    {
        for (const std::string_view name : names)
        {
            requests.push_back(app::ImageRequest{++ticket, path_of(name)});
        }
    }
    const Outcomes outcomes = read_all(requests);
    bool in_order = outcomes.size() == requests.size();
    for (std::size_t index = 0; in_order && index < outcomes.size(); ++index)
    {
        in_order = outcomes.at(index).ticket == requests.at(index).ticket;
    }
    expect(in_order, "18 mixed requests give exactly one result each, in the order asked");
}

// adapter が読みの途中で壊れても（窓が閉じる途中）、走っている仕事は閉じた棚を持ったまま戻り、ワー
// カーは止まる（止まらなければ CTest の制限時間で落ちる）。
void verify_destroyed_while_reading()
{
    adapters::Win32Worker worker;
    {
        adapters::Win32ImageAdapter images(worker);
        images.bind([] {});
        images.request(app::ImageRequest{1U, path_of("wide.png")});
        images.request(app::ImageRequest{2U, path_of("a.png")});
    }
    expect(worker.started(), "the requests started the worker and it stops with the adapter gone");
}

int run_tests()
{
    verify_not_loaded_before_a_request();
    expect(CreateDirectoryW(root, nullptr) != 0, "the sample folder is made");
    verify_unknown_needs_no_wic();
    verify_formats();
    verify_premultiplied_and_scaled();
    verify_content_decides_the_format();
    verify_broken_files();
    verify_limits_and_missing();
    verify_locked_file();
    verify_one_result_each();
    verify_destroyed_while_reading();
    std::printf("Image adapter: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
} // namespace

int main()
{
    const auto directory = nenenib::tests::temporary_test_directory(L"image");
    if (!directory.has_value())
    {
        std::fprintf(stderr, "FAIL: the temporary test folder is not made (error %lu)\n",
                     static_cast<unsigned long>(directory.error()));
        return 1;
    }
    // 見本を作る WIC の符号器のため。製品の UI スレッド（Main.cpp）と同じ STA にし、プロセスに
    // 長く生きる MTA を持たない（adapter はワーカーの仕事の中で 1 枚ごとに MTA を作って壊す・
    // V-1426 の軽い 1）。
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        std::fprintf(stderr, "FAIL: COM is not initialised\n");
        return 1;
    }
    const int result = run_tests();
    CoUninitialize();
    nenenib::tests::remove_temporary_test_directory(directory.value());
    return result;
}
