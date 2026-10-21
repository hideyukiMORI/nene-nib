// Markdown プレビューの 1 要素 1 layout の保持（ADR 0151 の決定 16・#784）の契約。
// 本物の DirectWrite で、同じ鍵は作り直さないこと・幅が変われば別の layout・高さが正で
// 見出しは倍率ぶん高いこと・装飾の範囲が UTF-8 のバイト位置から UTF-16 の位置へ写ること・
// 高さを測るだけでは保持しないこと・保持は 256 件まで・見えている要素だけを作って描くこと
// （替え玉の描画先 CountingTarget）を数える。#788 のレビュー R4 で、画像の枠の斜体と comment の
// 効果・HTML の等幅と折らないこと・字体の設定の変更で保持を捨てること・番号の記号の列と
// 1 行・種類の違うリストの境の間も数える。
// 字体は Segoe UI（Windows の既定で入っている）で、無い機械では測らない（QLT-013）。
#include "CountingTarget.hpp"
#include "DirectWriteFactory.hpp"
#include "WindowChecks.hpp"

#include "MarkdownDocument.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewFrame.hpp"
#include "PreviewInk.hpp"
#include "PreviewPainter.hpp"
#include "PreviewPane.hpp"
#include "PreviewPlacement.hpp"
#include "PreviewScene.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "RenderFailure.hpp"
#include "ThemeCatalog.hpp"
#include "ThemeName.hpp"

#include <array>
#include <combaseapi.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace win32 = nenenib::ui::win32;
using win32::PreviewTextLayouts;

constexpr float width = 400.0F;
constexpr std::size_t many = 300;

[[nodiscard]] win32::PreviewTextStyle style_of()
{
    return win32::PreviewTextStyle{"Segoe UI", 13.5F, 96};
}

[[nodiscard]] core::PreviewBlock block_of(core::PreviewBlockKind kind, std::string text,
                                          std::vector<core::PreviewSpan> spans = {})
{
    const std::uint8_t level = kind == core::PreviewBlockKind::heading ? 1 : 0;
    return core::PreviewBlock{
        kind, level, 0, 0, {}, core::Offset{0}, std::move(text), std::move(spans)};
}

void check_reuse()
{
    PreviewTextLayouts layouts;
    const auto block = block_of(core::PreviewBlockKind::paragraph, "reuse me");
    const auto first = layouts.get(block, width, style_of());
    const auto second = layouts.get(block, width, style_of());
    expect(first.has_value() && second.has_value() && first.value() == second.value(),
           "the same preview key returns the held layout");
    expect(layouts.created() == 1 && layouts.held() == 1,
           "the same preview key does not create a second layout");
    const auto wider = layouts.get(block, width * 2, style_of());
    expect(wider.has_value() && layouts.created() == 2 && layouts.held() == 2,
           "a different width is a different preview layout");
}

void check_heights()
{
    PreviewTextLayouts layouts;
    const auto paragraph =
        layouts.measure(block_of(core::PreviewBlockKind::paragraph, "text"), width, style_of());
    const auto heading =
        layouts.measure(block_of(core::PreviewBlockKind::heading, "text"), width, style_of());
    const auto code =
        layouts.measure(block_of(core::PreviewBlockKind::code, "x\ny\n"), width, style_of());
    expect(paragraph.has_value() && paragraph.value() > 0, "a preview paragraph has a height");
    expect(paragraph.has_value() && heading.has_value() && heading.value() > paragraph.value(),
           "an h1 preview heading is taller than a paragraph");
    expect(paragraph.has_value() && code.has_value() && code.value() < paragraph.value() * 2.5F &&
               code.value() > paragraph.value() * 1.5F,
           "a code block drops its final line feed (two rows)");
    expect(layouts.held() == 0 && layouts.created() == 3, "measuring does not hold layouts");
}

// "aé😀bcd": a=1・é=2・😀=4 バイト。UTF-16 では a=0・é=1・😀=2,3・b=4・c=5・d=6。
void check_span_ranges()
{
    PreviewTextLayouts layouts;
    const std::vector<core::PreviewSpan> spans{
        {core::Offset{7}, core::Offset{8}, core::PreviewStyle::strong},
        {core::Offset{8}, core::Offset{9}, core::PreviewStyle::link},
        {core::Offset{3}, core::Offset{7}, core::PreviewStyle::emphasis}};
    const auto layout = layouts.get(block_of(core::PreviewBlockKind::paragraph,
                                             "a\xC3\xA9\xF0\x9F\x98\x80"
                                             "bcd",
                                             spans),
                                    width, style_of());
    expect(layout.has_value(), "a decorated preview layout is created");
    if (!layout)
    {
        return;
    }
    auto &made = *layout.value().Get();
    DWRITE_FONT_WEIGHT bold = DWRITE_FONT_WEIGHT_NORMAL;
    DWRITE_FONT_WEIGHT plain = DWRITE_FONT_WEIGHT_BOLD;
    DWRITE_FONT_STYLE slanted = DWRITE_FONT_STYLE_NORMAL;
    DWRITE_FONT_STYLE upright = DWRITE_FONT_STYLE_ITALIC;
    BOOL underlined = FALSE;
    BOOL bare = TRUE;
    expect(SUCCEEDED(made.GetFontWeight(4, &bold)) && bold == DWRITE_FONT_WEIGHT_BOLD &&
               SUCCEEDED(made.GetFontWeight(3, &plain)) && plain == DWRITE_FONT_WEIGHT_NORMAL,
           "a strong span after a surrogate pair maps to UTF-16 position 4");
    expect(SUCCEEDED(made.GetFontStyle(2, &slanted)) && slanted == DWRITE_FONT_STYLE_ITALIC &&
               SUCCEEDED(made.GetFontStyle(1, &upright)) && upright == DWRITE_FONT_STYLE_NORMAL,
           "an emphasis span over a surrogate pair maps to UTF-16 positions 2 and 3");
    expect(SUCCEEDED(made.GetUnderline(5, &underlined)) && underlined != FALSE &&
               SUCCEEDED(made.GetUnderline(6, &bare)) && bare == FALSE,
           "a link span is underlined only over its own range");
}

void check_capacity()
{
    PreviewTextLayouts layouts;
    for (std::size_t index = 0; index < many; ++index)
    {
        const auto made = layouts.get(
            block_of(core::PreviewBlockKind::paragraph, std::to_string(index)), width, style_of());
        expect(made.has_value(), "each distinct preview block gets a layout");
    }
    expect(layouts.held() == PreviewTextLayouts::capacity,
           "preview layouts are held up to the capacity");
    const auto oldest =
        layouts.get(block_of(core::PreviewBlockKind::paragraph, "0"), width, style_of());
    expect(oldest.has_value() && layouts.created() == many + 1,
           "the oldest preview layout was dropped first");
}

void check_visible_only()
{
    core::MarkdownDocument document;
    for (std::size_t index = 0; index < many; ++index)
    {
        document.blocks.push_back(
            block_of(core::PreviewBlockKind::paragraph, "paragraph " + std::to_string(index)));
    }
    PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto tops = win32::PreviewBlockTops::measure(document, layouts, style, width);
    expect(tops.has_value() && tops.value().size() == many && layouts.held() == 0,
           "the preview block tops are measured without holding layouts");
    if (!tops)
    {
        return;
    }
    const auto scroll = tops.value().top(many / 2);
    expect(tops.value().first_visible(scroll) == many / 2,
           "the first visible preview block is found by its top");
    CountingTarget target;
    const win32::PreviewBrushes brushes;
    std::optional<win32::RenderFailure> failure;
    win32::PreviewPainter<CountingTarget> painter(target, layouts, brushes, failure);
    painter.paint(win32::PreviewScene{document, tops.value(), style,
                                      core::LayoutRect{0, 0, static_cast<std::int32_t>(width), 200},
                                      scroll});
    expect(!failure.has_value() && target.balanced(), "the preview paints without failure");
    expect(layouts.held() > 0 && layouts.held() < 10 && target.text_layouts() == layouts.held(),
           "only the visible preview blocks are created and drawn");
}
// メモリの bitmap のソフトウェアの描画先（brush を本物で作るため・SoftwareTarget と同じ作り方）。
[[nodiscard]] Microsoft::WRL::ComPtr<ID2D1RenderTarget>
wic_target(Microsoft::WRL::ComPtr<IWICBitmap> &bitmap)
{
    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    Microsoft::WRL::ComPtr<IWICImagingFactory> imaging;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> target;
    const auto properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf())) ||
        FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&imaging))) ||
        FAILED(imaging->CreateBitmap(400, 200, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad,
                                     &bitmap)) ||
        FAILED(factory->CreateWicBitmapRenderTarget(bitmap.Get(), properties, &target)))
    {
        return nullptr;
    }
    return target;
}

[[nodiscard]] bool effect_is(IDWriteTextLayout &layout, UINT32 position, ID2D1Brush *brush)
{
    Microsoft::WRL::ComPtr<IUnknown> effect;
    Microsoft::WRL::ComPtr<IUnknown> expected;
    return brush != nullptr && SUCCEEDED(layout.GetDrawingEffect(position, &effect)) &&
           SUCCEEDED(brush->QueryInterface(IID_PPV_ARGS(&expected))) &&
           effect.Get() == expected.Get();
}

// "x [画像: cat]": 画像の枠は 2 バイト目から 15 バイト目まで（UTF-16 では 2 から 12）。
void check_image_span()
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    win32::PreviewBrushes brushes;
    expect(target != nullptr && theme.has_value() &&
               SUCCEEDED(brushes.prepare(*target.Get(), theme.value().view())),
           "the preview brushes are made on a software target");
    if (target == nullptr || !theme)
    {
        return;
    }
    core::MarkdownDocument document;
    document.blocks.push_back(
        block_of(core::PreviewBlockKind::paragraph, "x [\xE7\x94\xBB\xE5\x83\x8F: cat]",
                 {{core::Offset{2}, core::Offset{15}, core::PreviewStyle::image}}));
    PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto tops = win32::PreviewBlockTops::measure(document, layouts, style, width);
    std::optional<win32::RenderFailure> failure;
    win32::PreviewPainter<ID2D1RenderTarget> painter(*target.Get(), layouts, brushes, failure);
    target->BeginDraw();
    painter.paint(
        win32::PreviewScene{document, tops.value(), style, core::LayoutRect{0, 0, 400, 200}, 0.0F});
    expect(SUCCEEDED(target->EndDraw()) && !failure.has_value(), "the image span is painted");
    const auto layout = layouts.get(document.blocks.front(), width - 32.0F, style);
    DWRITE_FONT_STYLE slanted = DWRITE_FONT_STYLE_NORMAL;
    expect(layout.has_value() && layouts.created() == 2 &&
               SUCCEEDED(layout.value()->GetFontStyle(3, &slanted)) &&
               slanted == DWRITE_FONT_STYLE_ITALIC,
           "the image frame is italic");
    expect(layout.has_value() &&
               effect_is(*layout.value().Get(), 3, brushes.of(win32::PreviewInk::comment)) &&
               !effect_is(*layout.value().Get(), 0, brushes.of(win32::PreviewInk::comment)),
           "the image frame is drawn in the comment ink");
}

void check_html_and_style()
{
    PreviewTextLayouts layouts;
    const auto block = block_of(core::PreviewBlockKind::html, "<div>\n</div>\n");
    const auto html = layouts.get(block, width, style_of());
    std::array<wchar_t, 64> family{};
    expect(html.has_value() && html.value()->GetWordWrapping() == DWRITE_WORD_WRAPPING_NO_WRAP &&
               SUCCEEDED(html.value()->GetFontFamilyName(0, family.data(),
                                                         static_cast<UINT32>(family.size()))) &&
               std::wstring_view(family.data()) != L"Segoe UI",
           "an HTML block is monospaced and does not wrap");
    auto larger = style_of();
    larger.points = 20.0F;
    const auto changed = layouts.get(block, width, larger);
    expect(changed.has_value() && layouts.held() == 1 && layouts.created() == 2,
           "a font size change drops the held preview layouts");
    const auto again = layouts.get(block, width, style_of());
    expect(again.has_value() && layouts.created() == 3,
           "the earlier font size is made again after a change");
}

[[nodiscard]] core::PreviewBlock item_of(std::string marker, std::string text)
{
    auto block = block_of(core::PreviewBlockKind::list_item, std::move(text));
    block.list_depth = 1;
    block.marker = std::move(marker);
    return block;
}

void check_markers()
{
    core::MarkdownDocument document;
    for (std::size_t number = 1; number <= 12; ++number)
    {
        document.blocks.push_back(item_of(std::to_string(number) + ".", "item"));
    }
    PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto tops = win32::PreviewBlockTops::measure(document, layouts, style, width);
    expect(tops.has_value() && tops.value().indent(0) == tops.value().indent(11) &&
               tops.value().indent(0) > 40.0F,
           "a numbered list widens its marker column to its widest number");
    const auto marker = layouts.get(win32::preview_marker_block(document.blocks.at(9)),
                                    win32::preview_marker_layout_width(), style);
    DWRITE_TEXT_METRICS metrics{};
    expect(marker.has_value() && SUCCEEDED(marker.value()->GetMetrics(&metrics)) &&
               metrics.lineCount == 1,
           "the marker 10. stays on one line");
    core::MarkdownDocument lists;
    lists.blocks = {item_of("\xE2\x80\xA2", "a"), item_of("\xE2\x80\xA2", "b"), item_of("1.", "c")};
    const auto mixed = win32::PreviewBlockTops::measure(lists, layouts, style, width);
    expect(mixed.has_value() && mixed.value().top(1) == mixed.value().bottom(0) &&
               mixed.value().top(2) > mixed.value().bottom(1),
           "items of one list touch and a different list starts after half a line");
}

// renderer が所有する other の枠のプレビュー（#801・ADR 0155）。PreviewFrame を持つ frame を
// 描いて落ちず、同じ文書を 2 回描いても layout を作り直さない（縦の位置も保持して測り直さない）。
// 最初の parse の前（文書が空）は地だけ。錨を含む要素の上端が上の送りで、最初の要素は 0。
void check_pane_reuse()
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    win32::PreviewPane pane;
    expect(target != nullptr && theme.has_value() &&
               pane.prepare(*target.Get(), theme.value().view()).has_value(),
           "the preview pane prepares its brushes on a software target");
    if (target == nullptr || !theme)
    {
        return;
    }
    auto document = std::make_shared<core::MarkdownDocument>();
    for (std::size_t index = 0; index < many; ++index)
    {
        auto block =
            block_of(core::PreviewBlockKind::paragraph, "paragraph " + std::to_string(index));
        block.source_begin = core::Offset{index * 16};
        document->blocks.push_back(std::move(block));
    }
    const std::shared_ptr<const core::MarkdownDocument> shown = document;
    const auto style = style_of();
    const core::LayoutRect area{0, 0, 400, 200};
    target->BeginDraw();
    const auto empty =
        pane.paint(*target.Get(), application::PreviewFrame{nullptr, {}, {}}, style, area);
    const auto first = pane.paint(
        *target.Get(), application::PreviewFrame{shown, core::Offset{40 * 16}, {}}, style, area);
    const auto made = pane.created();
    const auto second = pane.paint(
        *target.Get(), application::PreviewFrame{shown, core::Offset{40 * 16}, {}}, style, area);
    expect(SUCCEEDED(target->EndDraw()) && empty.has_value() && first.has_value() &&
               second.has_value(),
           "a frame with a preview paints without failure");
    expect(made > 0 && pane.created() == made,
           "the preview layouts are reused across frames for the same document");
    auto layouts = win32::PreviewTextLayouts{};
    const auto tops = win32::PreviewBlockTops::measure(*shown, layouts, style, 400.0F);
    expect(tops.has_value() &&
               win32::preview_scroll_of(*shown, tops.value(), core::Offset{40 * 16 + 3}) ==
                   tops.value().top(40) &&
               win32::preview_scroll_of(*shown, tops.value(), core::Offset{5}) == 0.0F,
           "the anchor scrolls to the top of the block that holds it");
}
} // namespace

void verify_preview_layouts()
{
    const auto factory = directwrite_factory();
    if (!factory || !installed(*factory.Get(), L"Segoe UI"))
    {
        unmeasured("#784 preview layouts need Segoe UI");
        return;
    }
    check_reuse();
    check_heights();
    check_span_ranges();
    check_capacity();
    check_visible_only();
    check_image_span();
    check_html_and_style();
    check_markers();
    check_pane_reuse();
}
} // namespace nenenib::tests::ui
