// Markdown プレビューの 1 要素 1 layout の保持（ADR 0151 の決定 16・#784）の契約。本物の
// DirectWrite で、同じ鍵は作り直さないこと・幅が変われば別の
// layout・高さが正で見出しは倍率ぶん高いこと・ 装飾の範囲が UTF-8 のバイト位置から UTF-16
// の位置へ写ること・高さを測るだけでは保持しないこと・ 保持は 256
// 件まで・見えている要素だけを作って描くこと（替え玉の描画先 CountingTarget）を数える。 字体は
// Segoe UI（Windows の既定で入っている）で、無い機械では測らない（QLT-013）。
#include "CountingTarget.hpp"
#include "DirectWriteFactory.hpp"
#include "WindowChecks.hpp"

#include "MarkdownDocument.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewPainter.hpp"
#include "PreviewScene.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "RenderFailure.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
}
} // namespace nenenib::tests::ui
