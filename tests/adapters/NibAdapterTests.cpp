// adapters/win32 のファイルと文字コードの往復を、ビルドディレクトリ配下の一時フォルダで測る
// （QLT-013 / ADR 0010）。中核の分岐カバレッジ（QLT-009）の対象ではない。
// 一時フォルダの名前は固定で、冒頭で消して作り直す。時刻も乱数も使わない（ARC-007）。
#include "AbsolutePath.hpp"
#include "CodePageFailure.hpp"
#include "CompleteFileRead.hpp"
#include "EncodingFailure.hpp"
#include "FileFailure.hpp"
#include "FileHandle.hpp"
#include "FilePath.hpp"
#include "FindHandle.hpp"
#include "Milestone.hpp"
#include "SettingsCodec.hpp"
#include "TemporaryFiles.hpp"
#include "TextEncoding.hpp"
#include "Win32CodePageAdapter.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32SettingsAdapter.hpp"
#include "Win32TimingAdapter.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <expected>
#include <format>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using nenenib::adapters::win32::absolute_file_path;
using nenenib::adapters::win32::decode_settings;
using nenenib::adapters::win32::encode_settings;
using nenenib::adapters::win32::FileHandle;
using nenenib::adapters::win32::FindHandle;
using nenenib::adapters::win32::reclaim_temporaries_beside;
using nenenib::adapters::win32::SettingsPaths;
using nenenib::adapters::win32::SettingsVersion;
using nenenib::adapters::win32::Win32CodePageAdapter;
using nenenib::adapters::win32::Win32FileAdapter;
using nenenib::adapters::win32::Win32SettingsAdapter;
using nenenib::adapters::win32::Win32TimingAdapter;
using nenenib::application::CodePageFailure;
using nenenib::application::FileFailure;
using nenenib::application::FileWriteMode;
using nenenib::application::SettingsFailure;
using nenenib::core::byte_order_mark;
using nenenib::core::default_editor_settings;
using nenenib::core::detect_encoding;
using nenenib::core::FilePath;
using nenenib::core::Milestone;
using nenenib::core::TextEncoding;

// application が渡す上限と同じ値（ADR 0010 の決定 12）。正本は EditorController にある。
constexpr std::size_t read_limit = 64U * 1024U * 1024U;
constexpr wchar_t folder[] = L"nib-adapter-files";
constexpr wchar_t recorded_marks[] = L"nib-adapter-files/timing.json";
constexpr wchar_t unrecorded_marks[] = L"nib-adapter-files/silent.json";
constexpr std::array<const wchar_t *, 12> file_names{
    L"nib-adapter-files/utf8.txt",
    L"nib-adapter-files/bom.txt",
    L"nib-adapter-files/sjis.txt",
    L"nib-adapter-files/replaced.txt",
    recorded_marks,
    unrecorded_marks,
    L"nib-adapter-files/settings/settings.v1",
    L"nib-adapter-files/settings/settings.v1.lock",
    L"nib-adapter-files/settings/settings.v2",
    L"nib-adapter-files/settings/settings.v2.lock",
    L"nib-adapter-files/settings/settings.v3",
    L"nib-adapter-files/settings/settings.v3.lock"};

std::size_t &failure_count()
{
    static std::size_t failures = 0;
    return failures;
}

std::size_t &check_count()
{
    static std::size_t checks = 0;
    return checks;
}

int test_result()
{
    return failure_count() == 0 ? 0 : 1;
}

void expect(bool condition, const char *description)
{
    ++check_count();
    if (!condition)
    {
        ++failure_count();
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

// 固定の名前なので、前回の実行が残していたものを消してから作り直す。
void reset_folder()
{
    for (const wchar_t *name : file_names)
    {
        DeleteFileW(name);
    }
    RemoveDirectoryW(L"nib-adapter-files/settings");
    RemoveDirectoryW(folder);
    const BOOL created = CreateDirectoryW(folder, nullptr);
    expect(created != 0 || GetLastError() == ERROR_ALREADY_EXISTS, "the temporary folder is there");
}

FilePath path_of(std::string_view name)
{
    auto parsed = FilePath::parse(name);
    expect(parsed.has_value(), "the test path is valid");
    return std::move(parsed).value();
}

std::wstring process_hex()
{
    return std::format(L"{:x}", GetCurrentProcessId());
}

std::wstring temporary_beside(std::wstring_view target, std::wstring_view tag)
{
    return std::wstring(target) + L".nib-tmp-" + std::wstring(tag);
}

bool file_exists(const std::wstring &name)
{
    return GetFileAttributesW(name.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// 別の保存が書いている途中の一時ファイル。共有 0 で開いたまま持つ。
FileHandle held_temporary(const std::wstring &name)
{
    return FileHandle(CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr));
}

using HeldNames = std::vector<std::unique_ptr<FileHandle>>;

// 新しい adapter の最初の保存が試す名前（連番 1〜8）を、共有 0 で開いたまま塞ぐ（ADR 0135 の
// 決定 2）。手放すと誰も開いていない残骸になり、次の保存の直前に回収される。
HeldNames occupy_first_names(std::wstring_view target)
{
    HeldNames held;
    for (int sequence = 1; sequence <= 8; ++sequence)
    {
        const std::wstring name =
            temporary_beside(target, process_hex() + L"-" + std::to_wstring(sequence));
        held.push_back(std::make_unique<FileHandle>(CreateFileW(
            name.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)));
        expect(held.back()->valid(), "a temporary name the save may try is held");
    }
    return held;
}

// 保存先の一時ファイル `<target>.nib-tmp-*` が 1 つも残っていない（ADR 0135）。
bool no_temporaries_of(std::wstring_view target)
{
    WIN32_FIND_DATAW data{};
    const FindHandle search(FindFirstFileW((std::wstring(target) + L".nib-tmp-*").c_str(), &data));
    return !search.valid();
}

void verify_round_trip(Win32FileAdapter &files, std::string_view name, std::string_view bytes,
                       TextEncoding expected)
{
    const FilePath path = path_of(name);
    expect(files.write(path, bytes).has_value(), "the bytes are written");
    const auto read = files.read(path, read_limit);
    expect(read.has_value() && read.value() == bytes, "the same bytes come back");
    const auto detected = detect_encoding(read.value_or(std::string{}));
    expect(detected.has_value() && detected.value() == expected, "the encoding is detected");
}

void verify_utf8_files(Win32FileAdapter &files)
{
    const std::string utf8 = "一行目\r\n二行目\r\n三行目";
    verify_round_trip(files, "nib-adapter-files/utf8.txt", utf8, TextEncoding::utf8);
    verify_round_trip(files, "nib-adapter-files/bom.txt", std::string(byte_order_mark()) + utf8,
                      TextEncoding::utf8_bom);
}

void verify_read_boundaries(Win32FileAdapter &files)
{
    const FilePath path = path_of("nib-adapter-files/utf8.txt");
    expect(files.write(path, "").has_value(), "an empty file can be written");
    const auto empty = files.read(path, 0);
    expect(empty.has_value() && empty.value().empty(), "zero-byte reads succeed at a zero limit");
    expect(files.write(path, "x").has_value(), "a one-byte file can be written");
    expect(files.read(path, 0).error() == FileFailure::too_large,
           "nonempty files are rejected before a zero-length read");
    expect(files.read(path, 1).value() == "x", "a one-byte read accepts the exact limit");
    const std::string bytes("a\0b\n", 4);
    expect(files.write(path, bytes).has_value(), "binary NUL bytes can be written");
    expect(files.read(path, bytes.size()).value() == bytes,
           "the initialized read length includes NUL and a trailing newline");
    expect(files.read(path, bytes.size() - 1).error() == FileFailure::too_large,
           "one byte above the limit is rejected");
    expect(files.write(path, "z").has_value() && files.read(path, 32).value() == "z",
           "a small file does not retain the previous read's longer tail");
}

void expect_read_steps(std::span<const std::expected<std::size_t, FileFailure>> steps,
                       bool succeeds)
{
    const std::string_view input("a\0bcde", 6);
    std::size_t calls = 0;
    std::size_t received = 0;
    const auto result = nenenib::adapters::win32::complete_file_read(
        input.size(),
        [&](std::span<char> target) noexcept -> std::expected<std::size_t, FileFailure>
        {
            expect(target.size() == input.size() - received,
                   "each read requests only the declared bytes still missing");
            const auto index = calls;
            ++calls;
            if (index >= steps.size())
            {
                return std::unexpected(FileFailure::unreadable);
            }
            const auto step = steps[index];
            if (!step)
            {
                return step;
            }
            const auto copied = (std::min)(step.value(), target.size());
            std::copy_n(input.data() + received, copied, target.data());
            received += copied;
            return step;
        });
    expect(calls == steps.size(), "completion or failure stops at the expected read call");
    if (succeeds)
    {
        expect(result.has_value() && result.value() == input,
               "complete reads preserve every byte including NUL in the correct position");
    }
    if (!succeeds)
    {
        expect(!result.has_value() && result.error() == FileFailure::unreadable,
               "an incomplete read returns unreadable without exposing partial bytes");
    }
}

void verify_complete_read_steps()
{
    using Step = std::expected<std::size_t, FileFailure>;
    const Step failure = std::unexpected(FileFailure::access_denied);
    expect_read_steps(std::array<Step, 1>{6}, true);
    expect_read_steps(std::array<Step, 2>{2, 4}, true);
    expect_read_steps(std::array<Step, 3>{1, 2, 3}, true);
    expect_read_steps(std::array<Step, 1>{0}, false);
    expect_read_steps(std::array<Step, 2>{2, 0}, false);
    expect_read_steps(std::array<Step, 1>{failure}, false);
    expect_read_steps(std::array<Step, 2>{2, failure}, false);
    expect_read_steps(std::array<Step, 1>{7}, false);
    expect_read_steps(std::array<Step, 2>{2, 5}, false);
    std::size_t calls = 0;
    const auto empty = nenenib::adapters::win32::complete_file_read(
        0,
        [&calls](std::span<char>) noexcept -> Step
        {
            ++calls;
            return std::unexpected(FileFailure::unreadable);
        });
    expect(empty.has_value() && empty.value().empty() && calls == 0,
           "an empty declaration succeeds without invoking the read operation");
}

void verify_read_request_size()
{
    using nenenib::adapters::win32::file_read_request_size;
    const auto maximum = static_cast<std::size_t>((std::numeric_limits<DWORD>::max)());
    expect(file_read_request_size(0) == 0, "zero remaining bytes request nothing");
    expect(file_read_request_size(read_limit) == read_limit,
           "a complete 64 MiB read keeps its single request");
    expect(file_read_request_size(maximum - 1) == maximum - 1,
           "one byte below DWORD maximum is not split");
    expect(file_read_request_size(maximum) == maximum, "DWORD maximum fits one read request");
    expect(file_read_request_size(maximum + 1) == maximum,
           "one byte above DWORD maximum is capped without allocating that input");
    expect(file_read_request_size((std::numeric_limits<std::size_t>::max)()) == maximum,
           "the request cap does not overflow at size_t maximum");
}

void verify_shift_jis_file(Win32FileAdapter &files, Win32CodePageAdapter &code_pages)
{
    const auto encoded = code_pages.from_utf8("日本語\n改行");
    expect(encoded.has_value(), "Japanese text fits in CP932");
    verify_round_trip(files, "nib-adapter-files/sjis.txt", encoded.value_or(std::string{}),
                      TextEncoding::shift_jis);
    const auto decoded = code_pages.to_utf8(encoded.value());
    expect(decoded.has_value() && decoded.value() == "日本語\n改行", "CP932 comes back as UTF-8");
}

void verify_replacement(Win32FileAdapter &files)
{
    const FilePath path = path_of("nib-adapter-files/replaced.txt");
    expect(files.write(path, "first content").has_value(), "the first content is written");
    expect(files.write(path, "second").has_value(), "the second content replaces it");
    const auto read = files.read(path, read_limit);
    expect(read.has_value() && read.value() == "second", "the replacement is what comes back");
    // 一時ファイルは置き換えのあとに残らない。
    expect(no_temporaries_of(L"nib-adapter-files/replaced.txt"), "the temporary file is gone");
}

void verify_failures(Win32FileAdapter &files)
{
    const auto missing = files.read(path_of("nib-adapter-files/absent.txt"), read_limit);
    expect(!missing.has_value() && missing.error() == FileFailure::not_found,
           "an absent path is not_found");
    const auto directory = files.read(path_of("nib-adapter-files"), read_limit);
    expect(!directory.has_value() && directory.error() == FileFailure::unreadable,
           "a folder is unreadable");
    const auto refused = files.write(path_of("nib-adapter-files/absent/deep.txt"), "x");
    expect(!refused.has_value(), "writing into a folder that is not there fails");
    // 上限は引数で来る。小さく渡せば、読む前に断ることが見える。
    const auto capped = files.read(path_of("nib-adapter-files/utf8.txt"), 4);
    expect(!capped.has_value() && capped.error() == FileFailure::too_large,
           "a file larger than the limit the caller passed is too_large");
    // 保存に上限は無い。開ける大きさを超えて貼り付けた本文も書ける。
    const FilePath big = path_of("nib-adapter-files/replaced.txt");
    expect(files.write(big, std::string(1024U * 1024U, 'z')).has_value(),
           "a body larger than the read limit still writes");
    expect(files.read(big, read_limit).value_or(std::string{}).size() == 1024U * 1024U,
           "and it comes back whole");
}

void verify_create_only(Win32FileAdapter &files)
{
    const auto path = path_of("nib-adapter-files/replaced.txt");
    // 前の版の固定名は今の一時ファイルの名前の形に当たらないので、使いも消しもしない。
    const auto legacy = path_of("nib-adapter-files/replaced.txt.nib-tmp");
    expect(DeleteFileW(L"nib-adapter-files/replaced.txt") != 0, "remove our previous fixture");
    expect(files.write(path, "new content", FileWriteMode::create_new).has_value(),
           "create_new places a complete new file");
    const auto refused = files.write(path, "replacement", FileWriteMode::create_new);
    expect(!refused && refused.error() == FileFailure::already_exists,
           "create_new refuses a file that already exists");
    expect(files.read(path, read_limit).value_or(std::string{}) == "new content",
           "a rejected create preserves all existing bytes");
    expect(no_temporaries_of(L"nib-adapter-files/replaced.txt"),
           "successful and rejected creation leave no temporary file");
    expect(DeleteFileW(L"nib-adapter-files/replaced.txt") != 0, "remove only our created file");
    expect(files.write(legacy, "someone else's temporary").has_value(),
           "prepare a file at the previous fixed temporary name");
    expect(files.write(path, "new content", FileWriteMode::create_new).has_value(),
           "the previous fixed temporary name does not block creation");
    expect(files.read(legacy, read_limit).value_or(std::string{}) == "someone else's temporary",
           "an unowned file at the previous fixed name is neither overwritten nor deleted");
    expect(DeleteFileW(L"nib-adapter-files/replaced.txt.nib-tmp") != 0,
           "remove only the fixture we prepared at the previous fixed name");
    expect(DeleteFileW(L"nib-adapter-files/replaced.txt") != 0, "remove our created file again");
}

void verify_resolved_write(Win32FileAdapter &files)
{
    const auto input = path_of("nib-adapter-files/../nib-adapter-files/replaced.txt");
    const auto absolute = files.resolve(input);
    expect(absolute.has_value(), "FilePort resolves relative names through the OS boundary");
    if (!absolute)
    {
        return;
    }
    expect(absolute.value().file_name() == "replaced.txt" &&
               absolute.value().text().find("..") == std::string_view::npos,
           "resolved names preserve the filename and remove dot segments");
    expect(files.write(absolute.value(), "resolved", FileWriteMode::create_new).has_value(),
           "the resolved path can be created");
    expect(files.read(input, read_limit).value_or(std::string{}) == "resolved",
           "relative and resolved paths read the same bytes");
}

void verify_segmented_write(Win32FileAdapter &files)
{
    const auto path = path_of("nib-adapter-files/replaced.txt");
    constexpr std::array<std::string_view, 5> parts{"", "a\r", "\n日", "本\n",
                                                    std::string_view("z\0", 2)};
    expect(files.write(path, parts, FileWriteMode::replace).has_value(),
           "byte fragments are written as one file");
    expect(files.read(path, read_limit).value_or(std::string{}) ==
               std::string("a\r\n日本\nz\0", 12),
           "fragment boundaries do not change UTF-8, CRLF or NUL bytes");
    expect(files.write(path, parts, FileWriteMode::create_new).error() ==
               FileFailure::already_exists,
           "segmented create_new preserves an existing target");
    const std::array<std::string_view, 0> empty{};
    expect(files.write(path, empty, FileWriteMode::replace).has_value() &&
               files.read(path, 0).value_or("not empty").empty(),
           "an empty segment list replaces with an empty file");
    expect(files.write(path, "resolved").has_value(), "restore the replacement fixture");
}

void verify_failed_replacement(Win32FileAdapter &files)
{
    const auto path = path_of("nib-adapter-files/replaced.txt");
    {
        const FileHandle held(CreateFileW(L"nib-adapter-files/replaced.txt", GENERIC_READ,
                                          FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
        expect(held.valid(), "hold the target without sharing replacement");
        constexpr std::array<std::string_view, 2> parts{"must not ", "replace"};
        expect(!files.write(path, parts, FileWriteMode::replace),
               "a locked target refuses replacement after all fragments are written");
    }
    expect(files.read(path, read_limit).value_or(std::string{}) == "resolved",
           "failed replacement retains the complete original file");
    expect(no_temporaries_of(L"nib-adapter-files/replaced.txt"),
           "failed replacement removes the temporary file it owned");
}

void verify_code_pages(Win32CodePageAdapter &code_pages)
{
    const auto empty = code_pages.from_utf8("");
    expect(empty.has_value() && empty.value().empty(), "an empty body encodes to nothing");
    const auto emoji = code_pages.from_utf8("😀");
    expect(!emoji.has_value() && emoji.error() == CodePageFailure::unencodable,
           "an emoji does not fit in CP932");
    const auto broken = code_pages.to_utf8("\xFF\xFF");
    expect(!broken.has_value() && broken.error() == CodePageFailure::undecodable,
           "bytes that are not CP932 are undecodable");
}

void verify_code_page_bytes(Win32CodePageAdapter &code_pages)
{
    constexpr std::array<std::pair<std::string_view, std::string_view>, 5> cases{{
        {"ASCII\r\n", "ASCII\r\n"},
        {"\x93\xfa\x96\x7b\x8c\xea", "日本語"},
        {"\xb6\xc5", "ｶﾅ"},
        {std::string_view{"a\0b", 3}, std::string_view{"a\0b", 3}},
        {std::string_view{"A\xb6\x93\xfa\0Z", 6}, std::string_view{"Aｶ日\0Z", 9}},
    }};
    for (const auto &[cp932, utf8] : cases)
    {
        const auto decoded = code_pages.to_utf8(cp932);
        expect(decoded && decoded.value() == utf8, "explicit CP932 bytes decode exactly");
        const auto encoded = code_pages.from_utf8(utf8);
        expect(encoded && encoded.value() == cp932, "explicit UTF-8 bytes encode exactly");
    }
    const auto empty = code_pages.to_utf8("");
    expect(empty && empty.value().empty(), "empty CP932 decoding succeeds");
    for (const auto broken : {"\x81\x30", "\x82", "A\x82"})
    {
        const auto decoded = code_pages.to_utf8(broken);
        expect(!decoded && decoded.error() == CodePageFailure::undecodable,
               "invalid pairs and trailing lead bytes are rejected");
    }
}

void verify_code_page_independent_calls(Win32CodePageAdapter &code_pages)
{
    std::string cp932;
    std::string utf8;
    for (std::size_t repeat = 0; repeat < 4096; ++repeat)
    {
        cp932 += "\x93\xfa\x96\x7b\x8c\xea";
        utf8 += "日本語";
    }
    const auto first = code_pages.to_utf8(cp932);
    const auto short_text = code_pages.to_utf8("x");
    const auto empty = code_pages.to_utf8("");
    const auto failed = code_pages.to_utf8("\x82");
    const auto repeated = code_pages.to_utf8(cp932);
    expect(first && first.value() == utf8, "long CP932 input has all expected UTF-8 bytes");
    expect(short_text && short_text.value() == "x", "short decoding retains no previous tail");
    expect(empty && empty.value().empty(), "empty decoding retains no previous tail");
    expect(!failed && failed.error() == CodePageFailure::undecodable,
           "a rejected call does not become an empty success");
    expect(repeated && repeated.value() == utf8 && first && first.value() == utf8,
           "repeated long decoding preserves independently owned previous results");
}

void verify_code_page_scope(Win32CodePageAdapter &code_pages)
{
    verify_code_pages(code_pages);
    verify_code_page_bytes(code_pages);
    verify_code_page_independent_calls(code_pages);
}

// 節目を積むのは bind のあとだけで、JSON はデストラクタが 1 度だけ書く（ADR 0011 の決定 2）。
void verify_timing_marks(Win32FileAdapter &files)
{
    {
        Win32TimingAdapter timing;
        timing.bind(recorded_marks);
        timing.mark(Milestone::input_received);
        timing.mark(Milestone::frame_presented);
    }
    {
        Win32TimingAdapter silent;
        silent.mark(Milestone::input_received);
    }
    const auto recorded = absolute_file_path(recorded_marks);
    const auto unrecorded = absolute_file_path(unrecorded_marks);
    expect(recorded.has_value() && unrecorded.has_value(), "both measurement paths resolve");
    if (!recorded.has_value() || !unrecorded.has_value())
    {
        return;
    }
    const auto bytes = files.read(recorded.value(), read_limit);
    expect(bytes.has_value(), "the bound adapter wrote its measurement file");
    const std::string_view text = bytes.has_value() ? std::string_view(bytes.value()) : "";
    expect(text.find("\"processCreationToFirstFrameMs\"") != std::string_view::npos,
           "the report opens with the process creation to first frame value");
    // プロセス生成 → bind は節目で測れないので、計測器はこの値を最初の区間に使う（Issue #19）。
    expect(text.find("\"processCreationToOriginMs\"") != std::string_view::npos,
           "the report carries the process creation to origin value");
    expect(text.find("\"qpcFrequency\"") != std::string_view::npos,
           "the report records the counter frequency");
    expect(text.find("\"input_received\"") != std::string_view::npos,
           "the input milestone is in the report");
    expect(text.find("\"frame_presented\"") != std::string_view::npos,
           "the presented milestone is in the report");
    expect(text.find("\"qpcMicroseconds\"") != std::string_view::npos,
           "each milestone carries its counter reading");
    const auto missing = files.read(unrecorded.value(), read_limit);
    expect(!missing.has_value() && missing.error() == FileFailure::not_found,
           "an adapter that was never bound writes nothing");
}

void verify_settings_codec()
{
    const auto defaults = default_editor_settings();
    const auto decoded = decode_settings(encode_settings(defaults), SettingsVersion::v3);
    expect(decoded.has_value() && same_settings(decoded.value(), defaults),
           "default settings round trip");
    const auto custom = decode_settings("\xEF\xBB\xBF\r\nversion = 1\r\n"
                                        " colorscheme = neutral-light \r\n"
                                        "\vfont_family = 日本語 Font=Test\r\nfont_size=21.25\f",
                                        SettingsVersion::v1);
    expect(custom.has_value(), "BOM, CRLF, whitespace and an equals sign in a font are accepted");
    if (!custom)
    {
        return;
    }
    expect(custom.value().font_family.text() == "日本語 Font=Test" &&
               custom.value().font_size.points() == 21.25F,
           "custom settings retain their values");
    const auto round_trip = decode_settings(encode_settings(custom.value()), SettingsVersion::v3);
    expect(round_trip.has_value() && same_settings(round_trip.value(), custom.value()),
           "custom settings round trip including an explicit theme");
}

void verify_bad_settings_fields()
{
    const std::string valid = encode_settings(default_editor_settings());
    for (const auto suffix : {"version=1\n", "unknown=yes\n", "broken\n"})
    {
        const auto result = decode_settings(valid + suffix, SettingsVersion::v3);
        expect(!result && result.error() == SettingsFailure::malformed,
               "duplicate keys, unknown keys and lines without equals are malformed");
    }
    const auto missing = decode_settings("version=1\n", SettingsVersion::v1);
    expect(!missing && missing.error() == SettingsFailure::malformed, "missing fields fail");
    const auto version =
        decode_settings("version=4\n" + valid.substr(valid.find('\n') + 1), SettingsVersion::v3);
    expect(!version && version.error() == SettingsFailure::unsupported_version,
           "an unknown version is incompatible");
    const auto theme = decode_settings("version=1\ncolorscheme=missing\nfont_family=Consolas\n"
                                       "font_size=13.5\n",
                                       SettingsVersion::v1);
    expect(!theme &&
               std::get<nenenib::core::ThemeLookupFailure>(theme.error()).name.text() == "missing",
           "unknown theme keeps its name");
}

void verify_bad_settings_values()
{
    for (const auto value : {"", "7.99", "40.01", "nan", "inf", "13,5", "13.5junk", "1e100"})
    {
        const auto result = decode_settings(
            std::string("version=1\ncolorscheme=system\nfont_family=Consolas\nfont_size=") + value,
            SettingsVersion::v1);
        expect(!result && result.error() == SettingsFailure::invalid_font_size,
               "invalid sizes and non-finite values are rejected");
    }
    for (const auto &value : {std::string{}, std::string("\xFF"), std::string("A\x01"),
                              std::string("A\0B", 3), std::string(257, 'a')})
    {
        const auto result = decode_settings(
            std::string("version=1\ncolorscheme=system\nfont_size=13.5\nfont_family=") + value,
            SettingsVersion::v1);
        expect(!result && result.error() == SettingsFailure::invalid_font_family,
               "invalid font names use the domain validator");
    }
}

void verify_settings_versions()
{
    auto settings = default_editor_settings();
    settings.guide = nenenib::core::GuideVisibility::hidden;
    settings.wrapping = nenenib::core::LineWrapping::no_wrap;
    const auto bytes = encode_settings(settings);
    const auto decoded = decode_settings(bytes, SettingsVersion::v3);
    expect(decoded && same_settings(decoded.value(), settings),
           "guide and wrapping round trip through v3");
    expect(bytes.starts_with("version=3\n") && bytes.ends_with("guide=off\nwrap=off\n"),
           "v3 writer emits the canonical six-field schema");
    const auto common = bytes.substr(bytes.find('\n') + 1);
    const auto v2 = "version=2\n" + common.substr(0, common.rfind("wrap="));
    const auto old = decode_settings(v2, SettingsVersion::v2);
    expect(old && old.value().guide == nenenib::core::GuideVisibility::hidden &&
               old.value().wrapping == nenenib::core::LineWrapping::wrap,
           "v2 preserves guide and initializes wrapping on");
    const auto extra = decode_settings(v2 + "wrap=off\n", SettingsVersion::v2);
    expect(!extra && extra.error() == SettingsFailure::malformed, "v2 rejects a wrap field");
    const auto no_wrap = bytes.substr(0, bytes.rfind("wrap="));
    expect(!decode_settings(no_wrap, SettingsVersion::v3), "v3 requires wrap");
    for (const auto value : {"", "true", "ON", "shown", "on off"})
    {
        const auto bad = decode_settings(no_wrap + "wrap=" + value + "\n", SettingsVersion::v3);
        expect(!bad && bad.error() == SettingsFailure::malformed, "invalid wrap value refused");
    }
    const auto prefix = v2.substr(0, v2.rfind("guide="));
    expect(!decode_settings(prefix, SettingsVersion::v2), "v2 still requires guide");
    for (const auto value : {"", "true", "ON", "shown", "on off"})
    {
        expect(!decode_settings(prefix + "guide=" + value + "\n", SettingsVersion::v2),
               "invalid v2 guide value refused");
    }
    expect(!decode_settings(bytes + "wrap=on\n", SettingsVersion::v3), "duplicate wrap refused");
    expect(!decode_settings(bytes + "guide=on\n", SettingsVersion::v3), "duplicate guide refused");
    const auto v1 = "version=1\n" + common.substr(0, common.rfind("guide="));
    expect(!decode_settings(v1 + "guide=on\n", SettingsVersion::v1), "v1 rejects guide");
    expect(!decode_settings(v1 + "wrap=on\n", SettingsVersion::v1), "v1 rejects wrap");
    const auto wrong_old = decode_settings(v1, SettingsVersion::v3);
    const auto wrong_new = decode_settings(bytes, SettingsVersion::v1);
    expect(!wrong_old && wrong_old.error() == SettingsFailure::unsupported_version && !wrong_new &&
               wrong_new.error() == SettingsFailure::unsupported_version,
           "expected filename version must match document version");
}

void verify_settings_restart(Win32FileAdapter &files)
{
    const auto path = path_of("nib-adapter-files/settings/settings.v3");
    Win32SettingsAdapter first(
        files, SettingsPaths{path, path_of("nib-adapter-files/settings/settings.v2"),
                             path_of("nib-adapter-files/settings/settings.v1")});
    const auto premature = first.write(default_editor_settings());
    expect(!premature && premature.error() == SettingsFailure::not_loaded,
           "writing before loading is refused");
    const auto missing = first.read();
    expect(missing.has_value() && !missing.value().has_value(), "missing settings are optional");
    expect(GetFileAttributesW(L"nib-adapter-files/settings") == INVALID_FILE_ATTRIBUTES,
           "reading defaults creates no settings directory");
    auto settings = default_editor_settings();
    settings.font_size = nenenib::core::FontSize::from_points(21.25F).value();
    expect(first.write(settings).has_value(), "first write creates the settings directory");
    Win32SettingsAdapter restarted(
        files, SettingsPaths{path, path_of("nib-adapter-files/settings/settings.v2"),
                             path_of("nib-adapter-files/settings/settings.v1")});
    const auto restored = restarted.read();
    expect(restored.has_value(), "a fresh adapter reads the saved file");
    const auto loaded = restored.value_or(std::nullopt);
    expect(loaded.has_value() && same_settings(loaded.value(), settings),
           "restart restores saved settings");
    expect(restarted.write(default_editor_settings()).has_value(), "another window can save");
    const auto stale = first.write(settings);
    expect(!stale && stale.error() == SettingsFailure::changed_externally,
           "stale settings never overwrite another window's update");
    expect(files.read(path, 4096).value_or("") == encode_settings(default_editor_settings()),
           "the competing update remains intact");
}

void verify_settings_lock(Win32FileAdapter &files)
{
    const auto path = path_of("nib-adapter-files/settings/settings.v3");
    Win32SettingsAdapter settings(
        files, SettingsPaths{path, path_of("nib-adapter-files/settings/settings.v2"),
                             path_of("nib-adapter-files/settings/settings.v1")});
    expect(settings.read().has_value(), "the settings adapter loads before lock checks");
    const auto before = files.read(path, 4096);
    {
        const FileHandle lock(CreateFileW(L"nib-adapter-files/settings/settings.v3.lock",
                                          GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
        expect(lock.valid(), "the competing lock is acquired");
        const auto result = settings.write(default_editor_settings());
        expect(!result && result.error() == SettingsFailure::unwritable,
               "a held lock refuses a write without waiting");
        expect(files.read(path, 4096) == before, "lock failure leaves the original bytes intact");
    }
    expect(settings.write(default_editor_settings()).has_value(), "a released lock allows retry");
    expect(SetFileAttributesW(L"nib-adapter-files/settings/settings.v3", FILE_ATTRIBUTE_READONLY) !=
               0,
           "the settings file becomes read only");
    const auto refused = settings.write(default_editor_settings());
    expect(!refused && refused.error() == SettingsFailure::unwritable,
           "read-only files refuse save");
    expect(files.read(path, 4096) == before, "failed replacement preserves settings bytes");
    expect(SetFileAttributesW(L"nib-adapter-files/settings/settings.v3", FILE_ATTRIBUTE_NORMAL) !=
               0,
           "the test restores the original attributes");
}

void verify_rejected_settings(Win32FileAdapter &files, std::string_view bytes,
                              SettingsFailure expected)
{
    const auto path = path_of("nib-adapter-files/settings/settings.v3");
    expect(files.write(path, bytes).has_value(), "the invalid settings fixture is written");
    Win32SettingsAdapter settings(
        files, SettingsPaths{path, path_of("nib-adapter-files/settings/settings.v2"),
                             path_of("nib-adapter-files/settings/settings.v1")});
    const auto loaded = settings.read();
    expect(!loaded && loaded.error() == expected, "invalid settings return their typed failure");
    const auto written = settings.write(default_editor_settings());
    expect(!written && written.error() == expected, "a failed load blocks later writes");
    expect(files.read(path, read_limit).value_or("") == bytes, "invalid settings remain untouched");
}

void verify_settings_failures(Win32FileAdapter &files)
{
    verify_rejected_settings(files, "invalid", SettingsFailure::malformed);
    verify_rejected_settings(files,
                             "version=1\ncolorscheme=system\nfont_family=Consolas\nfont_size=13.5",
                             SettingsFailure::unsupported_version);
    verify_rejected_settings(files, std::string(4097, 'x'), SettingsFailure::too_large);
    Win32SettingsAdapter unavailable(files, std::unexpected(SettingsFailure::location_unavailable));
    const auto failed = unavailable.read();
    expect(!failed && failed.error() == SettingsFailure::location_unavailable,
           "an unavailable location is distinct from missing settings");
    expect(!unavailable.write(default_editor_settings()), "an unavailable location cannot save");
    Win32SettingsAdapter directory(
        files, SettingsPaths{path_of("nib-adapter-files/settings"),
                             path_of("nib-adapter-files/settings/settings.v2"),
                             path_of("nib-adapter-files/settings/settings.v1")});
    const auto unreadable = directory.read();
    expect(!unreadable && unreadable.error() == SettingsFailure::unreadable,
           "a directory is not mistaken for missing settings");
    expect(!directory.write(default_editor_settings()), "unreadable settings cannot be replaced");
}

constexpr std::string_view previous_bytes =
    "version=1\ncolorscheme=neutral-light\nfont_family=Consolas\nfont_size=21.25\n";

SettingsPaths settings_paths()
{
    return {path_of("nib-adapter-files/settings/settings.v3"),
            path_of("nib-adapter-files/settings/settings.v2"),
            path_of("nib-adapter-files/settings/settings.v1")};
}

void reset_settings(Win32FileAdapter &files, std::optional<std::string_view> previous)
{
    static_cast<void>(DeleteFileW(L"nib-adapter-files/settings/settings.v3"));
    static_cast<void>(DeleteFileW(L"nib-adapter-files/settings/settings.v2"));
    static_cast<void>(DeleteFileW(L"nib-adapter-files/settings/settings.v1"));
    if (previous.has_value())
    {
        expect(files.write(settings_paths().legacy, previous.value()).has_value(),
               "prepare previous settings bytes");
    }
}

void verify_migration_success(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    Win32SettingsAdapter store(files, settings_paths());
    const auto reading = store.read();
    expect(reading && reading.value().has_value(), "v1 is read only when v2 is missing");
    auto loaded = reading.value_or(std::nullopt).value_or(default_editor_settings());
    expect(loaded.font_size.points() == 21.25F &&
               loaded.guide == nenenib::core::GuideVisibility::shown,
           "v1 retains settings and initializes shown guide");
    expect(!files.read(settings_paths().current, 4096), "loading v1 does not migrate eagerly");
    loaded.guide = nenenib::core::GuideVisibility::hidden;
    expect(store.write(loaded).has_value(), "first changed settings migrate into v3");
    expect(files.read(settings_paths().legacy, 4096).value_or("") == previous_bytes,
           "migration leaves v1 bytes unchanged");
    expect(files.read(settings_paths().current, 4096).value_or("") == encode_settings(loaded),
           "migration writes v3 including hidden guide");
    // Both reading and locking v1 now fail: current saves must not touch either.
    const FileHandle old_file(CreateFileW(L"nib-adapter-files/settings/settings.v1", GENERIC_WRITE,
                                          0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                          nullptr));
    const FileHandle old_lock(CreateFileW(L"nib-adapter-files/settings/settings.v1.lock",
                                          GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
    expect(old_file.valid() && old_lock.valid(), "hold both previous file and previous lock");
    expect(store.write(default_editor_settings()).has_value(),
           "successful migration stops reading and locking previous settings");
    Win32SettingsAdapter restarted(files, settings_paths());
    expect(restarted.read().has_value() && restarted.write(loaded).has_value(),
           "existing v2 wins even when v1 cannot be read or locked");
}

void verify_migration_conflict(Win32FileAdapter &files, std::optional<std::string_view> before,
                               std::optional<std::string_view> after)
{
    reset_settings(files, before);
    Win32SettingsAdapter store(files, settings_paths());
    expect(store.read().has_value(), "load migration snapshot before external v1 edit");
    if (after.has_value())
    {
        expect(files.write(settings_paths().legacy, after.value()).has_value(),
               "old editor creates or changes v1");
    }
    if (!after.has_value())
    {
        expect(DeleteFileW(L"nib-adapter-files/settings/settings.v1") != 0,
               "old settings are deleted externally");
    }
    auto changed = default_editor_settings();
    changed.guide = nenenib::core::GuideVisibility::hidden;
    const auto result = store.write(changed);
    expect(!result && result.error() == SettingsFailure::changed_externally,
           "v1 change creation and deletion each block migration");
    expect(!files.read(settings_paths().current, 4096), "refused migration creates no v3");
    const auto remaining = files.read(settings_paths().legacy, 4096);
    expect(after.has_value() ? remaining && remaining.value() == after.value() : !remaining,
           "refused migration preserves externally edited v1 bytes");
}

void verify_migration_locks(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    Win32SettingsAdapter store(files, settings_paths());
    expect(store.read().has_value(), "load migration before lock checks");
    for (const auto name : {L"nib-adapter-files/settings/settings.v3.lock",
                            L"nib-adapter-files/settings/settings.v2.lock",
                            L"nib-adapter-files/settings/settings.v1.lock"})
    {
        const FileHandle held(CreateFileW(name, GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
        expect(held.valid(), "hold each migration lock independently");
        const auto result = store.write(default_editor_settings());
        expect(!result && result.error() == SettingsFailure::unwritable,
               "either migration lock blocks saving without waiting");
        expect(!files.read(settings_paths().current, 4096) &&
                   files.read(settings_paths().legacy, 4096).value_or("") == previous_bytes,
               "lock refusal leaves current absent and previous bytes intact");
    }
    const FileHandle previous(CreateFileW(L"nib-adapter-files/settings/settings.v1", GENERIC_WRITE,
                                          0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                          nullptr));
    expect(previous.valid(), "hold v1 unreadable after snapshot");
    const auto result = store.write(default_editor_settings());
    expect(!result && result.error() == SettingsFailure::unreadable,
           "migration snapshot read failure blocks save");
    expect(!files.read(settings_paths().current, 4096), "read failure creates no v3");
}

void verify_previous_rejection(Win32FileAdapter &files, std::string_view invalid,
                               SettingsFailure failure)
{
    reset_settings(files, invalid);
    Win32SettingsAdapter store(files, settings_paths());
    const auto reading = store.read();
    const auto writing = store.write(default_editor_settings());
    expect(!reading && reading.error() == failure && !writing && writing.error() == failure,
           "invalid v1 load blocks all later writes");
    expect(!files.read(settings_paths().current, 4096) &&
               files.read(settings_paths().legacy, read_limit).value_or("") == invalid,
           "invalid previous bytes remain untouched and current remains absent");
}

void verify_migration_current_created(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    Win32SettingsAdapter store(files, settings_paths());
    expect(store.read().has_value(), "load previous before another window creates v2");
    const auto external = encode_settings(default_editor_settings());
    expect(files.write(settings_paths().current, external).has_value(),
           "another window creates v2");
    const auto result = store.write(default_editor_settings());
    expect(!result && result.error() == SettingsFailure::changed_externally,
           "new current settings block stale migration");
    expect(files.read(settings_paths().current, 4096).value_or("") == external &&
               files.read(settings_paths().legacy, 4096).value_or("") == previous_bytes,
           "refused migration preserves both external current and original previous");
}

void verify_migration_write_failure(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    // 新しい adapter の最初の保存が試す 8 つの名前を、共有 0 で開いたまま塞ぐ（ADR 0135）。
    Win32FileAdapter fresh;
    Win32SettingsAdapter store(fresh, settings_paths());
    expect(store.read().has_value(), "load previous before failed migration write");
    {
        const HeldNames occupied = occupy_first_names(L"nib-adapter-files/settings/settings.v3");
        const auto result = store.write(default_editor_settings());
        expect(!result && result.error() == SettingsFailure::unwritable,
               "failed atomic migration write is reported");
        expect(!files.read(settings_paths().current, 4096) &&
                   files.read(settings_paths().legacy, 4096).value_or("") == previous_bytes,
               "failed migration write preserves current absence and previous bytes");
    }
    expect(store.write(default_editor_settings()).has_value(), "failed write keeps retry snapshot");
    expect(no_temporaries_of(L"nib-adapter-files/settings/settings.v3"),
           "the retry reclaims the temporary names that are no longer held");
}

constexpr std::string_view v2_bytes =
    "version=2\ncolorscheme=system\nfont_family=Consolas\nfont_size=18\nguide=off\n";

void verify_v2_migration(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    expect(files.write(settings_paths().previous, v2_bytes).has_value(),
           "prepare v2 migration source");
    Win32SettingsAdapter store(files, settings_paths());
    const auto reading = store.read();
    const auto loaded = reading.value_or(std::nullopt).value_or(default_editor_settings());
    expect(reading && loaded.font_size.points() == 18 &&
               loaded.guide == nenenib::core::GuideVisibility::hidden &&
               loaded.wrapping == nenenib::core::LineWrapping::wrap,
           "v2 takes precedence over v1 and initializes wrapping");
    expect(!files.read(settings_paths().current, 4096), "reading v2 does not migrate eagerly");
    auto changed = loaded;
    changed.wrapping = nenenib::core::LineWrapping::no_wrap;
    expect(store.write(changed).has_value(), "v2 can migrate to v3 with wrapping disabled");
    expect(files.read(settings_paths().previous, 4096).value_or("") == v2_bytes &&
               files.read(settings_paths().legacy, 4096).value_or("") == previous_bytes,
           "v3 migration preserves both older files byte for byte");
    const auto saved = decode_settings(files.read(settings_paths().current, 4096).value_or(""),
                                       SettingsVersion::v3);
    expect(saved && same_settings(saved.value(), changed), "migrated v3 holds the chosen wrapping");
    const FileHandle old_lock(CreateFileW(L"nib-adapter-files/settings/settings.v2.lock",
                                          GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
    expect(old_lock.valid() && store.write(loaded).has_value(),
           "after migration v2 lock is irrelevant");
}

void verify_v2_conflicts(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    Win32SettingsAdapter old(files, settings_paths());
    expect(old.read().has_value(), "load v1 while v2 and v3 are absent");
    expect(files.write(settings_paths().previous, v2_bytes).has_value(),
           "another version creates v2");
    const auto raced = old.write(default_editor_settings());
    expect(!raced && raced.error() == SettingsFailure::changed_externally &&
               !files.read(settings_paths().current, 4096),
           "an intervening v2 creation blocks migration from v1");
    Win32SettingsAdapter active(files, settings_paths());
    expect(active.read().has_value(), "load the existing v2 snapshot");
    expect(files.write(settings_paths().previous, std::string(v2_bytes) + "\n").has_value(),
           "externally change v2 bytes");
    const auto changed = active.write(default_editor_settings());
    expect(!changed && changed.error() == SettingsFailure::changed_externally,
           "v2 changes block stale migration");
    Win32SettingsAdapter removed(files, settings_paths());
    expect(removed.read().has_value(), "load updated v2 snapshot");
    expect(DeleteFileW(L"nib-adapter-files/settings/settings.v2") != FALSE, "remove v2 externally");
    const auto erased = removed.write(default_editor_settings());
    expect(!erased && erased.error() == SettingsFailure::changed_externally,
           "v2 deletion blocks stale migration");
}

void verify_v2_rejection(Win32FileAdapter &files)
{
    reset_settings(files, previous_bytes);
    expect(files.write(settings_paths().previous, "invalid").has_value(), "prepare malformed v2");
    Win32SettingsAdapter broken(files, settings_paths());
    const auto reading = broken.read();
    expect(!reading && reading.error() == SettingsFailure::malformed,
           "malformed v2 never falls back to valid v1");
    expect(!broken.write(default_editor_settings()) &&
               files.read(settings_paths().previous, 4096).value_or("") == "invalid",
           "malformed v2 blocks writes and preserves the source");
    expect(files.write(settings_paths().previous, v2_bytes).has_value(), "restore valid v2");
    Win32SettingsAdapter current(files, settings_paths());
    expect(current.read().has_value(), "load v2 before a lower-version change");
    expect(files.write(settings_paths().legacy, "invalid").has_value(), "change lower v1 version");
    expect(current.write(default_editor_settings()).has_value(),
           "a v1 change does not invalidate the higher v2 snapshot");
}

void verify_settings_migration(Win32FileAdapter &files)
{
    verify_v2_migration(files);
    verify_v2_conflicts(files);
    verify_v2_rejection(files);
    verify_migration_success(files);
    verify_migration_conflict(
        files, previous_bytes,
        "version=1\ncolorscheme=system\nfont_family=Consolas\nfont_size=18\n");
    verify_migration_conflict(files, std::nullopt, previous_bytes);
    verify_migration_conflict(files, previous_bytes, std::nullopt);
    verify_migration_locks(files);
    verify_migration_current_created(files);
    verify_migration_write_failure(files);
    verify_previous_rejection(files, "invalid", SettingsFailure::malformed);
    verify_previous_rejection(files, encode_settings(default_editor_settings()),
                              SettingsFailure::unsupported_version);
    verify_previous_rejection(files, std::string(previous_bytes) + "guide=on\n",
                              SettingsFailure::malformed);
    verify_previous_rejection(files, std::string(4097, 'x'), SettingsFailure::too_large);
    reset_settings(files, previous_bytes);
    verify_rejected_settings(files, "invalid", SettingsFailure::malformed);
    // Valid v1 must not conceal an unreadable current path.
    const FileHandle current(CreateFileW(L"nib-adapter-files/settings/settings.v3", GENERIC_WRITE,
                                         0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                         nullptr));
    Win32SettingsAdapter blocked(files, settings_paths());
    const auto reading = blocked.read();
    expect(current.valid() && !reading && reading.error() == SettingsFailure::unreadable,
           "unreadable v3 never falls back to valid v1");
    expect(!blocked.write(default_editor_settings()), "unreadable v3 blocks later writes");
}

void verify_absolute_path()
{
    const auto absolute = absolute_file_path(L"nib-adapter-files/utf8.txt");
    expect(absolute.has_value(), "a relative argument becomes an absolute path");
    if (!absolute.has_value())
    {
        return;
    }
    expect(absolute.value().file_name() == "utf8.txt", "the file name survives");
    expect(absolute.value().text().find(':') != std::string_view::npos,
           "the absolute path carries a drive");
}
// 同じファイルの判定（ADR 0056 の決定 5）。大文字と小文字だけが違う絶対パスは同じ、違う
// ファイルは違う。短い名前（8.3）とリンクは解かない（ADR の「残る穴」）。
void verify_same_file(const Win32FileAdapter &files)
{
    const auto lower = FilePath::parse("C:\\work\\note.txt");
    const auto upper = FilePath::parse("C:\\WORK\\NOTE.TXT");
    const auto other = FilePath::parse("C:\\work\\note2.txt");
    expect(lower.has_value() && upper.has_value() && other.has_value(), "the paths parse");
    if (!lower.has_value() || !upper.has_value() || !other.has_value())
    {
        return;
    }
    expect(files.same_file(lower.value(), upper.value()),
           "paths that differ only in case are the same file");
    expect(files.same_file(lower.value(), lower.value()), "a path is the same file as itself");
    expect(!files.same_file(lower.value(), other.value()), "different paths are different files");
}
// 一時ファイルの名前と回収（ADR 0135 の決定 5）。adapter は保存ごとに連番を進めるので、名前を
// 予言する試験は新しい adapter を使う（最初の保存の連番は 1）。
constexpr wchar_t temporaries_folder[] = L"nib-adapter-files/temporaries";
constexpr wchar_t kept_target[] = L"nib-adapter-files/temporaries/kept.txt";
constexpr std::array<const wchar_t *, 7> temporaries_fixtures{
    kept_target,
    L"nib-adapter-files/temporaries/settings.v3",
    L"nib-adapter-files/temporaries/settings.v3.lock",
    L"nib-adapter-files/temporaries/settings.v2.lock",
    L"nib-adapter-files/temporaries/settings.v1.lock",
    L"nib-adapter-files/temporaries/kept.txt.nib-tmp",
    L"nib-adapter-files/temporaries/notes.nib-tmp"};

// クラッシュや電源断で残った一時ファイル。誰も開いていない。
void leave_residue(const std::wstring &name)
{
    const FileHandle made(held_temporary(name));
    expect(made.valid(), "a residue temporary file is prepared");
}

// 試験が作った名前だけを消す。残ったものがあればフォルダを消せずに落ちる。
void remove_temporaries_folder(std::span<const std::wstring> created)
{
    for (const wchar_t *name : temporaries_fixtures)
    {
        static_cast<void>(DeleteFileW(name));
    }
    for (const std::wstring &name : created)
    {
        static_cast<void>(DeleteFileW(name.c_str()));
    }
    expect(RemoveDirectoryW(temporaries_folder) != 0,
           "the temporaries folder holds nothing the test did not name");
}

// 決定 5 (a)(c): 同じ保存先の一時ファイルが別の名前で同時に在り得る。ほかの保存が共有 0 で
// 開いたまま持つ一時ファイル（前の版の固定名・別のプロセス・プロセス ID の再利用）があっても、
// 自分の一時ファイルを CREATE_NEW で作って保存が通り、相手のものは消さない。
void verify_concurrent_temporaries(std::span<const std::wstring> others)
{
    Win32FileAdapter fresh;
    const auto target = path_of("nib-adapter-files/temporaries/kept.txt");
    {
        const FileHandle legacy(held_temporary(others[0]));
        const FileHandle other(held_temporary(others[1]));
        const FileHandle reused(held_temporary(others[2]));
        expect(legacy.valid() && other.valid() && reused.valid(),
               "other savers hold their temporary files without sharing");
        expect(fresh.write(target, "first").has_value(),
               "a save succeeds while other savers hold temporary files for the same target");
        expect(fresh.write(target, "second").has_value(),
               "a replacing save also succeeds beside live temporary files");
    }
    expect(fresh.read(target, read_limit).value_or(std::string{}) == "second",
           "the save beside live temporary files placed its own bytes");
    expect(file_exists(others[0]) && file_exists(others[1]) && file_exists(others[2]),
           "temporary files held open by other savers are not reclaimed");
}

// 決定 5 (b): 誰も開いていない残骸は、その保存先の保存の直前に消える。
void verify_reclaimed_residue(const std::wstring &residue)
{
    Win32FileAdapter fresh;
    leave_residue(residue);
    expect(
        fresh.write(path_of("nib-adapter-files/temporaries/kept.txt"), "after residue").has_value(),
        "a save succeeds beside a residue temporary file");
    expect(!file_exists(residue), "a residue nobody holds is reclaimed before the save");
}

// 決定 5 (d): 設定の保存も同じ adapter を通るので、設定の置き場の残骸も保存の直前に消える。
void verify_reclaimed_settings_residue(const std::wstring &residue)
{
    Win32FileAdapter fresh;
    leave_residue(residue);
    Win32SettingsAdapter store(fresh,
                               SettingsPaths{path_of("nib-adapter-files/temporaries/settings.v3"),
                                             path_of("nib-adapter-files/temporaries/settings.v2"),
                                             path_of("nib-adapter-files/temporaries/settings.v1")});
    expect(store.read().has_value(), "absent settings read before the save");
    expect(store.write(default_editor_settings()).has_value(), "the settings are saved");
    expect(!file_exists(residue), "a settings residue is reclaimed by the settings save");
}

// 決定 2: 試す 8 つの名前がすべて生きている一時ファイルなら、保存は unwritable で断り、
// 保存先も相手の一時ファイルも変えない。
void verify_exhausted_names()
{
    Win32FileAdapter fresh;
    const auto target = path_of("nib-adapter-files/temporaries/kept.txt");
    {
        const HeldNames occupied = occupy_first_names(kept_target);
        const auto refused = fresh.write(target, "must not replace");
        expect(!refused && refused.error() == FileFailure::unwritable,
               "a save refuses when every temporary name it may try is taken");
        expect(fresh.read(target, read_limit).value_or(std::string{}) == "after residue",
               "the refused save keeps the complete previous file");
    }
    expect(fresh.write(target, "after release").has_value(),
           "the next save tries new names and reclaims the released ones");
    expect(no_temporaries_of(kept_target), "released temporary names are reclaimed");
}

// 決定 3 の (b): 起動時に設定の置き場の `*.nib-tmp-*` を回収する。開いたままのものと、
// 名前の形に当たらないもの（前の版の固定名）は残す。
void verify_reclaimed_beside(std::span<const std::wstring> residues)
{
    constexpr wchar_t legacy[] = L"nib-adapter-files/temporaries/notes.nib-tmp";
    leave_residue(residues[0]);
    leave_residue(residues[1]);
    leave_residue(legacy);
    {
        const FileHandle live(held_temporary(residues[2]));
        expect(live.valid(), "a live temporary file beside the settings is held");
        reclaim_temporaries_beside(path_of("nib-adapter-files/temporaries/settings.v3"));
        expect(!file_exists(residues[0]) && !file_exists(residues[1]),
               "start-up reclaims every residue beside the settings");
        expect(file_exists(residues[2]), "start-up keeps a temporary file someone holds");
    }
    expect(file_exists(legacy), "start-up leaves names outside the temporary pattern");
}

void verify_temporary_files()
{
    const BOOL created = CreateDirectoryW(temporaries_folder, nullptr);
    expect(created != 0 || GetLastError() == ERROR_ALREADY_EXISTS,
           "the temporaries folder is there");
    const std::array<std::wstring, 8> named{
        std::wstring(kept_target) + L".nib-tmp",
        temporary_beside(kept_target, L"deadbeef-1"),
        temporary_beside(kept_target, process_hex() + L"-1"),
        temporary_beside(kept_target, L"deadbeef-2"),
        temporary_beside(L"nib-adapter-files/temporaries/settings.v3", L"deadbeef-3"),
        temporary_beside(L"nib-adapter-files/temporaries/history.v1", L"deadbeef-4"),
        temporary_beside(L"nib-adapter-files/temporaries/session.v2", process_hex() + L"-9"),
        temporary_beside(L"nib-adapter-files/temporaries/bookmarks.v1", L"deadbeef-5")};
    for (const std::wstring &name : named)
    {
        static_cast<void>(DeleteFileW(name.c_str()));
    }
    verify_concurrent_temporaries(std::span(named).first(3));
    verify_reclaimed_residue(named[3]);
    verify_reclaimed_settings_residue(named[4]);
    verify_exhausted_names();
    verify_reclaimed_beside(std::span(named).subspan(5));
    remove_temporaries_folder(named);
}

void verify_files(Win32FileAdapter &files, Win32CodePageAdapter &code_pages)
{
    verify_utf8_files(files);
    verify_shift_jis_file(files, code_pages);
    verify_replacement(files);
    verify_failures(files);
    verify_create_only(files);
    verify_resolved_write(files);
    verify_segmented_write(files);
    verify_failed_replacement(files);
    verify_code_page_scope(code_pages);
    verify_absolute_path();
    verify_same_file(files);
    verify_read_boundaries(files);
    verify_complete_read_steps();
    verify_read_request_size();
    verify_temporary_files();
}

bool known_scope(std::string_view scope)
{
    return scope == "--settings-codec" || scope == "--settings" || scope == "--files" ||
           scope == "--code-pages";
}

int run_memory_tests(std::string_view scope)
{
    if (scope == "--code-pages")
    {
        Win32CodePageAdapter code_pages;
        verify_code_page_scope(code_pages);
        std::printf("Code page adapter: %zu checks, %zu failures\n", check_count(),
                    failure_count());
        return test_result();
    }
    verify_settings_codec();
    verify_bad_settings_fields();
    verify_bad_settings_values();
    verify_settings_versions();
    std::printf("Settings codec: %zu checks, %zu failures\n", check_count(), failure_count());
    return test_result();
}
} // namespace

int main(int argc, char **argv)
{
    const std::string_view scope = argc == 2 ? argv[1] : "";
    if (argc > 2 || (argc == 2 && !known_scope(scope)))
    {
        std::fprintf(stderr, "Unknown adapter test scope\n");
        return 1;
    }
    if (scope == "--code-pages" || scope == "--settings-codec")
    {
        return run_memory_tests(scope);
    }
    reset_folder();
    Win32FileAdapter files;
    if (scope == "--settings")
    {
        verify_settings_restart(files);
        verify_settings_lock(files);
        verify_settings_failures(files);
        verify_settings_migration(files);
        std::printf("Settings adapter: %zu checks, %zu failures\n", check_count(), failure_count());
        return test_result();
    }
    Win32CodePageAdapter code_pages;
    verify_files(files, code_pages);
    if (scope == "--files")
    {
        std::printf("File adapter tests: %zu checks, %zu failures\n", check_count(),
                    failure_count());
        return test_result();
    }
    verify_timing_marks(files);
    verify_settings_codec();
    verify_bad_settings_fields();
    verify_bad_settings_values();
    verify_settings_versions();
    verify_settings_restart(files);
    verify_settings_lock(files);
    verify_settings_failures(files);
    verify_settings_migration(files);
    if (failure_count() != 0)
    {
        std::fprintf(stderr, "Nib adapter tests: %zu of %zu checks failed\n", failure_count(),
                     check_count());
        return 1;
    }
    std::printf("Nib adapter tests passed: %zu checks over files, code pages and paths\n",
                check_count());
    return 0;
}
