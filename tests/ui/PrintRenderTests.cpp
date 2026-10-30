// 印刷の紙面（#1047・ADR 0177 の決定 3〜5・8）。1 ページの命令をメモリの WIC bitmap に描いた画素で
// ヘッダの位置・行番号・折り返し・構文の色を確かめ、同じ紙面を print control からメモリの XPS へ
// 書いてページの数と範囲の選択を確かめる。紙の大きさは DEVMODE の純関数。実機のプリンタ・ファイル・
// 窓は使わない（ADR 0077）。字形はフォントに依るので、文字組みが作れない機械では測らない。
#include "MemoryPackageTarget.hpp"
#include "MemoryXpsTarget.hpp"
#include "WindowChecks.hpp"

#include "PrintComposition.hpp"
#include "PrintPageDrawing.hpp"
#include "PrintPaperSizes.hpp"
#include "PrintRendering.hpp"
#include "PrintSurface.hpp"

#include "DevicePixels.hpp"
#include "DisplayText.hpp"
#include "FontSize.hpp"
#include "Language.hpp"
#include "LineEnding.hpp"
#include "PrintJob.hpp"
#include "PrintPages.hpp"
#include "PrintPaper.hpp"
#include "PrintRange.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <d2d1_1.h>
#include <memory>
#include <objbase.h>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Pixels = std::vector<BYTE>;
using Microsoft::WRL::ComPtr;

constexpr std::uint8_t ink_level = 200;
constexpr int colored_spread = 60;
constexpr std::size_t channels = 4;
// A4 縦（210 × 297 mm）の DIP。
constexpr application::PrintPaper a4{794, 1123};

[[nodiscard]] application::PrintJob job_of(std::string text, core::Language language, bool gutter)
{
    // 題名と字体を先に作り、本文の共有は最後に作る（途中の value() の前に確保しない）。
    auto title = core::DisplayText::parse("sample.cpp").value();
    auto family = core::DisplayText::parse("Consolas").value();
    auto body = std::make_shared<const std::string>(std::move(text));
    return application::PrintJob{std::move(body), core::LineEnding::lf, std::move(title),
                                 language,        std::move(family),    core::default_font_size(),
                                 gutter};
}

// メモリの WIC bitmap の描画先の device（tests/ui の SoftwareTarget と同じ土台・GPU を使わない）。
[[nodiscard]] std::optional<ComPtr<ID2D1DeviceContext>> page_context(IWICBitmap *bitmap,
                                                                     IWICImagingFactory *imaging)
{
    ComPtr<ID2D1Factory1> factory;
    ComPtr<ID2D1RenderTarget> target;
    ComPtr<ID2D1DeviceContext> context;
    if (bitmap == nullptr || imaging == nullptr ||
        FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1),
                                 nullptr, &factory)) ||
        FAILED(factory->CreateWicBitmapRenderTarget(bitmap, D2D1::RenderTargetProperties(),
                                                    &target)) ||
        FAILED(target.As(&context)))
    {
        return std::nullopt;
    }
    return context;
}

[[nodiscard]] ComPtr<IWICImagingFactory2> imaging_factory()
{
    ComPtr<IWICImagingFactory2> imaging;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&imaging))))
    {
        return {};
    }
    return imaging;
}

// 1 ページを紙の大きさの bitmap に描いた画素（BGRA・行の幅は紙の幅）。描けなければ空。
[[nodiscard]] Pixels page_pixels(win32::PrintComposition &composition, std::size_t index)
{
    const auto imaging = imaging_factory();
    ComPtr<IWICBitmap> bitmap;
    const auto width = static_cast<UINT>(a4.width);
    const auto height = static_cast<UINT>(a4.height);
    if (!imaging || FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA,
                                                 WICBitmapCacheOnLoad, &bitmap)))
    {
        return {};
    }
    const auto context = page_context(bitmap.Get(), imaging.Get());
    if (!context.has_value())
    {
        return {};
    }
    context.value()->BeginDraw();
    const auto drawn = win32::draw_print_page(*context.value().Get(), composition.page(index));
    Pixels pixels(std::size_t{width} * height * channels);
    if (!drawn || FAILED(context.value()->EndDraw()) ||
        FAILED(bitmap->CopyPixels(nullptr, width * channels, static_cast<UINT>(pixels.size()),
                                  pixels.data())))
    {
        return {};
    }
    return pixels;
}

// 画素が字の色（どれかの成分が ink_level 未満）か、色のついた（成分の差が colored_spread を超える）
// 画素か。
[[nodiscard]] bool marked(const Pixels &pixels, std::size_t at, bool colored)
{
    const int blue = pixels.at(at);
    const int green = pixels.at(at + 1);
    const int red = pixels.at(at + 2);
    const int low = std::min({blue, green, red});
    return colored ? std::max({blue, green, red}) - low > colored_spread : low < ink_level;
}

// 矩形の中の、字の色（colored なら色のついた）画素の数。
[[nodiscard]] std::size_t count_in(const Pixels &pixels, const core::LayoutRect &area, bool colored)
{
    std::size_t count = 0;
    const auto stride = static_cast<std::size_t>(a4.width);
    for (auto y = std::max(area.top, 0); y < std::min(area.bottom, a4.height); ++y)
    {
        for (auto x = std::max(area.left, 0); x < std::min(area.right, a4.width); ++x)
        {
            const auto at =
                (static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x)) * channels;
            count += marked(pixels, at, colored) ? 1U : 0U;
        }
    }
    return count;
}

[[nodiscard]] core::LayoutRect content_row(const core::BodyLayout &body, std::int32_t row)
{
    const auto top = body.content.top + row * body.line_height;
    return core::LayoutRect{body.content.left, top, body.content.right, top + body.line_height};
}

void verify_rows_fit()
{
    auto composition =
        win32::PrintComposition::compose(job_of("a", core::Language::plain, true), a4);
    expect(composition.has_value(), "the print composition is made for A4");
    if (!composition)
    {
        return;
    }
    const auto &body = composition.value().body();
    const auto bottom =
        a4.height - static_cast<std::int32_t>(core::print_margin(core::reference_dpi));
    const auto rows = static_cast<std::int32_t>(composition.value().rows_per_page());
    // 1 ページの最後の行は下の余白の上に収まり、もう 1 行は入らない（行の高さは UNIFORM の整数）。
    expect(body.content.top + rows * body.line_height <= bottom,
           "the last printed row stays above the bottom margin");
    expect(body.content.top + (rows + 1) * body.line_height > bottom,
           "one more row would cross the bottom margin");
    expect(body.content.left > body.gutter.left && body.gutter.right > body.gutter.left,
           "the gutter keeps the body width rule");
}

void verify_page_pixels()
{
    const std::string text = "int main() { return \"string\"; }\n" + std::string(400, 'x') + "\n";
    auto composition =
        win32::PrintComposition::compose(job_of(text, core::Language::cpp, true), a4);
    // 行番号の無い紙は本文が左の余白から始まるので、頭に空白を置いた行で欄の位置に字が無いことを見る。
    auto bare = win32::PrintComposition::compose(
        job_of(std::string(40, ' ') + "x", core::Language::plain, false), a4);
    if (!composition || !bare)
    {
        unmeasured("print composition (DirectWrite)");
        return;
    }
    const auto pixels = page_pixels(composition.value(), 0);
    const auto plain = page_pixels(bare.value(), 0);
    if (pixels.empty() || plain.empty())
    {
        unmeasured("print page pixels (WIC)");
        return;
    }
    const auto &body = composition.value().body();
    const auto margin = static_cast<std::int32_t>(core::print_margin(core::reference_dpi));
    const core::LayoutRect header{margin, margin, a4.width - margin, margin + body.line_height};
    const auto quarter = core::width_of(header) / 4;
    expect(count_in(pixels, {header.left, header.top, header.left + quarter, header.bottom},
                    false) > 0,
           "the header shows the title on the left");
    expect(count_in(pixels, {header.right - quarter / 2, header.top, header.right, header.bottom},
                    false) > 0,
           "the header shows the page number on the right");
    const core::LayoutRect number{body.gutter.left, body.content.top, body.gutter.right,
                                  body.content.top + body.line_height};
    expect(count_in(pixels, number, false) > 0, "the gutter shows the line number");
    expect(count_in(plain, number, false) == 0 &&
               bare.value().body().content.left == bare.value().body().band.left,
           "no line number is drawn without the gutter");
    expect(count_in(pixels, content_row(body, 0), true) > 0,
           "the C++ line is drawn with syntax colors");
    expect(count_in(pixels, content_row(body, 2), false) > 0,
           "the long line wraps at the paper width");
    expect(count_in(pixels, content_row(body, 1), true) == 0,
           "the wrapped plain characters stay black");
}

void verify_pagination()
{
    std::string text;
    for (int line = 0; line < 200; ++line)
    {
        text += "line\n";
    }
    auto composition =
        win32::PrintComposition::compose(job_of(text, core::Language::plain, true), a4);
    if (!composition)
    {
        unmeasured("print pagination (DirectWrite)");
        return;
    }
    const auto rows = composition.value().rows_per_page();
    // 200 行と末尾の改行の後の空の行で 201 行。
    expect(composition.value().page_count() == (201 + rows - 1) / rows,
           "the pages are the rows divided by the rows per page");
    const auto second = composition.value().page(1);
    expect(second.number == 2 && second.count == composition.value().page_count() &&
               second.page.first_row == rows,
           "the second page continues after the first page rows");
}

// XPS の package の 1 つ目の文書のページの数。読めなければ nullopt。
[[nodiscard]] std::optional<UINT32> xps_pages(IXpsOMObjectFactory &factory, IStream &stream)
{
    const LARGE_INTEGER origin{};
    ComPtr<IXpsOMPackage> package;
    ComPtr<IXpsOMDocumentSequence> sequence;
    ComPtr<IXpsOMDocumentCollection> documents;
    ComPtr<IXpsOMDocument> document;
    ComPtr<IXpsOMPageReferenceCollection> references;
    UINT32 count = 0;
    if (FAILED(stream.Seek(origin, STREAM_SEEK_SET, nullptr)) ||
        FAILED(factory.CreatePackageFromStream(&stream, FALSE, &package)) ||
        FAILED(package->GetDocumentSequence(&sequence)) ||
        FAILED(sequence->GetDocuments(&documents)) || FAILED(documents->GetAt(0, &document)) ||
        FAILED(document->GetPageReferences(&references)) || FAILED(references->GetCount(&count)))
    {
        return std::nullopt;
    }
    return count;
}

using Printed = std::pair<std::optional<UINT32>, unsigned>;

// 紙面を print control からメモリの XPS へ書いた答え（書けたページの数と、受け口の Cancel
// の回数）。 writable が false なら XPS
// の書き手を作れない受け口（途中の失敗の道）。土台が作れなければ nullopt。
[[nodiscard]] std::optional<Printed>
print_to_memory(win32::PrintComposition &composition,
                std::span<const application::PrintRange> ranges, bool writable)
{
    const auto imaging = imaging_factory();
    ComPtr<IWICBitmap> bitmap;
    ComPtr<IXpsOMObjectFactory> xps;
    ComPtr<IStream> stream;
    ComPtr<ID2D1Device> device;
    if (!imaging ||
        FAILED(imaging->CreateBitmap(1, 1, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad,
                                     &bitmap)) ||
        FAILED(CoCreateInstance(__uuidof(XpsOMObjectFactory), nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&xps))) ||
        FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)))
    {
        return std::nullopt;
    }
    const auto context = page_context(bitmap.Get(), imaging.Get());
    if (!context.has_value())
    {
        return std::nullopt;
    }
    context.value()->GetDevice(&device);
    auto target = Microsoft::WRL::Make<MemoryPackageTarget>(
        Microsoft::WRL::Make<MemoryXpsTarget>(xps.Get(), writable ? stream.Get() : nullptr).Get());
    const auto rendered = win32::render_print_job(win32::PrintSurface{device, imaging},
                                                  *target.Get(), composition, ranges);
    return Printed{rendered ? xps_pages(*xps.Get(), *stream.Get()) : std::nullopt,
                   target->cancelled()};
}

void verify_xps_pages()
{
    std::string text;
    for (int line = 0; line < 120; ++line)
    {
        text += "int value = 1;\n";
    }
    auto composition =
        win32::PrintComposition::compose(job_of(text, core::Language::cpp, true), a4);
    if (!composition)
    {
        unmeasured("print to an in-memory XPS package (DirectWrite)");
        return;
    }
    const std::vector<application::PrintRange> second_page{{2, 2}};
    const auto all = print_to_memory(composition.value(), {}, true);
    const auto second = print_to_memory(composition.value(), second_page, true);
    const auto refused = print_to_memory(composition.value(), {}, false);
    if (!all.has_value() || !second.has_value() || !refused.has_value() ||
        !all.value().first.has_value() || !second.value().first.has_value())
    {
        unmeasured("print to an in-memory XPS package (print control)");
        return;
    }
    expect(composition.value().page_count() > 1, "the XPS sample spans more than one page");
    expect(all.value().first.value() == composition.value().page_count() && all.value().second == 0,
           "every page is written to the XPS package and the job is not cancelled");
    expect(second.value().first.value() == 1, "a page range writes only the chosen page");
    expect(!refused.value().first.has_value() && refused.value().second == 1,
           "a failure after the job target exists cancels the job once");
}

// 1 ページで見る行はそのページと交わる行だけ（二分探索・R4-1）。1 画面行の行が 1,000 本なら、
// どのページも見る行の数はページの行数と同じで、全ページで全行をちょうど 1 回ずつ見る。
void verify_page_lines()
{
    std::string text;
    for (int line = 0; line < 999; ++line)
    {
        text += "a\n";
    }
    auto composition =
        win32::PrintComposition::compose(job_of(text, core::Language::plain, true), a4);
    if (!composition)
    {
        unmeasured("print page lines (DirectWrite)");
        return;
    }
    std::size_t seen = 0;
    bool proportional = true;
    for (std::size_t index = 0; index < composition.value().page_count(); ++index)
    {
        const auto input = composition.value().page(index);
        const auto lines = win32::print_page_lines(input.lines, input.page);
        proportional = proportional && lines.size() == input.page.row_count &&
                       (lines.empty() || lines.front().first_row == input.page.first_row);
        seen += lines.size();
    }
    expect(proportional, "a page visits only the lines of its own rows");
    expect(seen == 1000, "the pages visit every line exactly once");
}

[[nodiscard]] DEVMODEW mode_of(DWORD fields, short paper, short orientation)
{
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    mode.dmFields = fields;
    mode.dmPaperSize = paper;
    mode.dmOrientation = orientation;
    return mode;
}

void verify_paper_sizes()
{
    const auto a4_portrait =
        win32::print_paper(mode_of(DM_PAPERSIZE | DM_ORIENTATION, DMPAPER_A4, DMORIENT_PORTRAIT));
    expect(a4_portrait.width == 794 && a4_portrait.height == 1123, "A4 portrait is 794 x 1123 DIP");
    const auto letter = win32::print_paper(
        mode_of(DM_PAPERSIZE | DM_ORIENTATION, DMPAPER_LETTER, DMORIENT_LANDSCAPE));
    expect(letter.width == 1056 && letter.height == 816, "Letter landscape swaps the sides");
    auto custom = mode_of(DM_PAPERSIZE | DM_PAPERWIDTH | DM_PAPERLENGTH, DMPAPER_A4, 0);
    custom.dmPaperWidth = 1000;
    custom.dmPaperLength = 2000;
    const auto measured = win32::print_paper(custom);
    expect(measured.width == 378 && measured.height == 756,
           "the paper width and length win over the paper number");
    const auto unknown = win32::print_paper(DEVMODEW{});
    expect(unknown.width == 794 && unknown.height == 1123, "an empty DEVMODE falls back to A4");
}
} // namespace

void verify_print_render()
{
    verify_paper_sizes();
    verify_rows_fit();
    verify_page_pixels();
    verify_pagination();
    verify_xps_pages();
    verify_page_lines();
}
} // namespace nenenib::tests::ui
