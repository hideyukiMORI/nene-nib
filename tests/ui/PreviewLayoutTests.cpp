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

#include "LayoutRect.hpp"
#include "MarkdownDocument.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "Palette.hpp"
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
#include "PreviewViewport.hpp"
#include "RenderFailure.hpp"
#include "RgbColor.hpp"
#include "Theme.hpp"
#include "ThemeCatalog.hpp"
#include "ThemeName.hpp"

#include <array>
#include <combaseapi.h>
#include <cstddef>
#include <cstdint>
#include <limits>
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
// 文書の全体を測る範囲（最初の要素から高さの上限なし）。
constexpr win32::PreviewViewport whole{0, width, std::numeric_limits<float>::max()};

[[nodiscard]] win32::PreviewTextStyle style_of()
{
    return win32::PreviewTextStyle{"Segoe UI", 13.5F, 96};
}

[[nodiscard]] core::PreviewBlock block_of(core::PreviewBlockKind kind, std::string text,
                                          std::vector<core::PreviewSpan> spans = {})
{
    const std::uint8_t level = kind == core::PreviewBlockKind::heading ? 1 : 0;
    return core::PreviewBlock{
        kind, level, 0, 0, {}, core::Offset{0}, std::move(text), std::move(spans), std::nullopt};
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
    const auto tops = win32::PreviewBlockTops::measure(document, layouts, style, whole);
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
    const auto tops = win32::PreviewBlockTops::measure(document, layouts, style, whole);
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
    const auto tops = win32::PreviewBlockTops::measure(document, layouts, style, whole);
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
    const auto mixed = win32::PreviewBlockTops::measure(lists, layouts, style, whole);
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
    expect(win32::preview_anchor_block(*shown, core::Offset{40 * 16 + 3}) == 40 &&
               win32::preview_anchor_block(*shown, core::Offset{5}) == 0,
           "the anchor picks the block that holds it");
}

// 枠の絵の使い回し（#903・ADR 0155 の追補）の文書: 見出し・段落（行内コードとリンク）・コード・
// 項目を繰り返す。source_begin は 16 バイトずつ。
[[nodiscard]] std::shared_ptr<const core::MarkdownDocument> picture_document()
{
    auto document = std::make_shared<core::MarkdownDocument>();
    for (std::size_t index = 0; index < 60; ++index)
    {
        const auto number = std::to_string(index);
        const auto kind = index % 4;
        auto block =
            kind == 0   ? block_of(core::PreviewBlockKind::heading, "Heading " + number)
            : kind == 1 ? block_of(core::PreviewBlockKind::paragraph, "code x and link " + number,
                                   {{core::Offset{5}, core::Offset{6}, core::PreviewStyle::code},
                                    {core::Offset{11}, core::Offset{15}, core::PreviewStyle::link}})
            : kind == 2 ? block_of(core::PreviewBlockKind::code, "int x = " + number + ";\n")
                        : item_of("\xE2\x80\xA2", "item " + number);
        block.source_begin = core::Offset{index * 16};
        document->blocks.push_back(std::move(block));
    }
    return document;
}

[[nodiscard]] std::vector<std::uint8_t> pixels_of(IWICBitmap &bitmap)
{
    constexpr UINT stride = 400 * 4;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(stride) * 200);
    if (FAILED(bitmap.CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data())))
    {
        return {};
    }
    return pixels;
}

// 同じ文書・錨 4・枠を PreviewPainter で別の WIC の描画先に直接描いた画素（比べる相手）。
[[nodiscard]] std::vector<std::uint8_t> directly_painted(const core::MarkdownDocument &document,
                                                         const core::Theme &theme,
                                                         const core::LayoutRect &area)
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    PreviewTextLayouts layouts;
    win32::PreviewBrushes brushes;
    const auto style = style_of();
    const auto tops = win32::PreviewBlockTops::measure(
        document, layouts, style,
        win32::PreviewViewport{4, static_cast<float>(core::width_of(area)),
                               static_cast<float>(core::height_of(area))});
    if (target == nullptr || !tops || FAILED(brushes.prepare(*target.Get(), theme)))
    {
        return {};
    }
    std::optional<win32::RenderFailure> failure;
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    win32::PreviewPainter<ID2D1RenderTarget> painter(*target.Get(), layouts, brushes, failure);
    painter.paint(win32::PreviewScene{document, tops.value(), style, area, 0.0F});
    if (FAILED(target->EndDraw()) || failure.has_value())
    {
        return {};
    }
    return pixels_of(*bitmap.Get());
}

// 鍵が同じ 2 frame 目は要素を描かず DrawBitmap 1 回・錨が変われば描き直す（bitmap はそのまま）・
// 枠の大きさが変われば bitmap を作り直す・テーマが変われば描き直す。bitmap を経た画素は同じ文書を
// 直接描いた画素と 1 画素も違わない（枠の位置は整数の画素で、文字の AA も同じ）。
void check_pane_picture()
{
    Microsoft::WRL::ComPtr<IWICBitmap> held_bitmap;
    const auto held = wic_target(held_bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    const auto other = catalog.find(core::ThemeName::parse("solarized-light").value());
    win32::PreviewPane pane;
    if (held == nullptr || !theme || !other ||
        !pane.prepare(*held.Get(), theme.value().view()).has_value())
    {
        expect(false, "the preview pane prepares its brushes for the picture checks");
        return;
    }
    const auto document = picture_document();
    const auto style = style_of();
    const core::LayoutRect area{10, 20, 390, 180};
    const application::PreviewFrame at_four{document, core::Offset{4 * 16}, {}};
    held->BeginDraw();
    held->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    const auto first = pane.paint(*held.Get(), at_four, style, area);
    const auto second = pane.paint(*held.Get(), at_four, style, area);
    expect(SUCCEEDED(held->EndDraw()) && first.has_value() && second.has_value(),
           "the held preview picture paints without failure");
    expect(pane.pictures() == 1 && pane.painted() == 1 && pane.reused() == 1,
           "the same key draws no blocks and copies the held picture once");
    const auto through = pixels_of(*held_bitmap.Get());
    expect(!through.empty() && through == directly_painted(*document, theme.value().view(), area),
           "the held preview picture has the same pixels as painting directly");
    held->BeginDraw();
    const auto moved = pane.paint(
        *held.Get(), application::PreviewFrame{document, core::Offset{8 * 16}, {}}, style, area);
    expect(moved.has_value() && pane.painted() == 2 && pane.pictures() == 1,
           "another anchor draws the blocks again into the same bitmap");
    const auto resized =
        pane.paint(*held.Get(), at_four, style, core::LayoutRect{10, 20, 390, 150});
    expect(resized.has_value() && pane.painted() == 3 && pane.pictures() == 2,
           "another frame size makes a new bitmap");
    const auto themed = pane.prepare(*held.Get(), other.value().view());
    const auto recoloured =
        themed ? pane.paint(*held.Get(), at_four, style, core::LayoutRect{10, 20, 390, 150})
               : themed;
    expect(SUCCEEDED(held->EndDraw()) && recoloured.has_value() && pane.painted() == 4 &&
               pane.pictures() == 2 && pane.reused() == 1,
           "another theme draws the blocks again");
}

// 測るのは錨の要素から枠の高さまでだけ（#811 のレビュー R1）。10 万要素の文書で錨が末尾の近くでも、
// 作る layout は枠の高さ ÷ 最小の行高 + 1 以下で、錨の要素の上端が枠の上端。
void check_measured_from_anchor()
{
    constexpr std::size_t huge = 100'000;
    constexpr float height = 200.0F;
    core::MarkdownDocument document;
    document.blocks.reserve(huge);
    for (std::size_t index = 0; index < huge; ++index)
    {
        document.blocks.push_back(
            block_of(core::PreviewBlockKind::paragraph, "paragraph " + std::to_string(index)));
    }
    PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto first = huge - 10;
    const auto tops = win32::PreviewBlockTops::measure(
        document, layouts, style, win32::PreviewViewport{first, width, height});
    const auto bound = static_cast<std::size_t>(height / win32::preview_line_pixels(style)) + 1;
    expect(tops.has_value() && tops.value().first() == first && tops.value().top(first) == 0.0F &&
               tops.value().size() > first && tops.value().size() <= huge,
           "the tops start at the anchor block at the top of the frame");
    expect(layouts.created() > 0 && layouts.created() <= bound,
           "only the blocks within the frame height are measured");
}

// リストの区切りは錨の 1 つ前から前へ決め、記号の列はリストの残り（上限 10,000 項目）で決める
// （#813・ADR 0155 の追補）。10 万項目の 1 つの番号つきリストで、読む要素は測る範囲＋錨の 1 つ前
// ＋リストの残り（上限）以内で、測った項目の字下げは揃う。12 項目のリストは錨を 2. に送っても
// 字下げが 10. の桁のまま動かない。
void check_list_scanned_in_range()
{
    constexpr std::size_t huge = 100'000;
    constexpr float height = 200.0F;
    core::MarkdownDocument document;
    document.blocks.reserve(huge);
    for (std::size_t index = 0; index < huge; ++index)
    {
        document.blocks.push_back(item_of(std::to_string(index + 1) + ".", "item"));
    }
    PreviewTextLayouts layouts;
    const auto style = style_of();
    // 2 つ目の錨は 9998. から 10000. へ桁が増え、広がった列で錨から置き直す。
    for (const std::size_t first : {huge - 1'000, std::size_t{9'997}})
    {
        const auto tops = win32::PreviewBlockTops::measure(
            document, layouts, style, win32::PreviewViewport{first, width, height});
        expect(tops.has_value() && tops.value().size() > first + 3 &&
                   tops.value().scanned() <=
                       tops.value().size() - first + 1 + win32::PreviewBlockTops::list_scan_limit &&
                   tops.value().scanned() <= huge - first + 1,
               "a list is read from the block before the anchor to its rest up to the limit");
        expect(tops.has_value() && tops.value().size() > first + 3 &&
                   tops.value().indent(first) == tops.value().indent(tops.value().size() - 1),
               "the measured items of one list share one marker column");
    }
    core::MarkdownDocument twelve;
    for (std::size_t number = 1; number <= 12; ++number)
    {
        twelve.blocks.push_back(item_of(std::to_string(number) + ".", "item"));
    }
    constexpr float short_frame = 80.0F;
    const auto top = win32::PreviewBlockTops::measure(
        twelve, layouts, style, win32::PreviewViewport{0, width, short_frame});
    const auto scrolled = win32::PreviewBlockTops::measure(
        twelve, layouts, style, win32::PreviewViewport{1, width, short_frame});
    expect(top.has_value() && scrolled.has_value() && top.value().size() < 10 &&
               scrolled.value().size() < 10 &&
               top.value().indent(1) == scrolled.value().indent(1) && top.value().indent(1) > 40.0F,
           "scrolling a list whose 10. is below the frame keeps its marker column");
}

// CR だけの改行の文書でも錨は本文の順に並んだ要素から引く（空の項目が末尾を指さない・#887）。
void check_anchor_after_carriage_returns()
{
    const auto parsed = core::parse_markdown("- foo\r-\r- bar", core::MarkdownStop::never());
    expect(parsed.has_value() && parsed.value().blocks.size() == 3 &&
               win32::preview_anchor_block(parsed.value(), core::Offset{3}) == 0 &&
               win32::preview_anchor_block(parsed.value(), core::Offset{6}) == 1 &&
               win32::preview_anchor_block(parsed.value(), core::Offset{11}) == 2,
           "the anchor follows the caret through a document with CR line endings");
}

// タスクリスト（ADR 0155 の追補 22〜24・#1101）の文書: 未チェック・チェック・点の項目。
[[nodiscard]] std::shared_ptr<const core::MarkdownDocument> task_document()
{
    auto parsed =
        core::parse_markdown("- [ ] open\n- [x] done\n- plain", core::MarkdownStop::never());
    if (!parsed)
    {
        return nullptr;
    }
    return std::make_shared<const core::MarkdownDocument>(std::move(parsed).value());
}

// チェックボックスの矩形は記号の列の右端（文字の左端から記号と文字の間）に一辺の正方形で、押す
// 範囲は 1 行の高さに左右 2 DIP。タスクでない項目は矩形を持たない。替え玉の描画先では、タスクの
// 項目は枠を 1 つ・チェックは線を 2 本描き、記号の layout を作らない（点の項目だけが作る）。
void check_task_boxes()
{
    const auto document = task_document();
    if (document == nullptr || document->blocks.size() != 3)
    {
        expect(false, "the task document parses into three items");
        return;
    }
    PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto tops = win32::PreviewBlockTops::measure(*document, layouts, style, whole);
    if (!tops.has_value())
    {
        expect(false, "the task document measures");
        return;
    }
    const auto &measured = tops.value();
    const auto side = static_cast<std::int32_t>(win32::preview_task_side(style));
    const auto line = static_cast<std::int32_t>(win32::preview_line_pixels(style) + 0.5F);
    const auto &open = measured.task_box(0);
    expect(open.has_value() && measured.task_box(1).has_value() &&
               !measured.task_box(2).has_value(),
           "only the task items keep a check box");
    if (!open.has_value())
    {
        return;
    }
    const auto right =
        static_cast<std::int32_t>(measured.indent(0) - win32::preview_marker_gap(96) + 0.5F);
    expect(core::width_of(open.value().square) == side &&
               core::height_of(open.value().square) == side && open.value().square.right == right,
           "the check box is a square of the side and ends at the marker gap");
    expect(
        core::height_of(open.value().target) == line &&
            core::width_of(open.value().target) == line + 4 &&
            core::contains(open.value().target, open.value().square.left, open.value().square.top),
        "the press target is one line high with 2 DIP on each side around the square");
    expect(measured.indent(0) == measured.indent(2) &&
               measured.indent(0) >= static_cast<float>(side) + 8.0F,
           "the marker column holds the check box and the bullet alike");
    CountingTarget target;
    win32::PreviewBrushes brushes;
    std::optional<win32::RenderFailure> failure;
    PreviewTextLayouts counted;
    const auto painted = win32::PreviewBlockTops::measure(*document, counted, style, whole);
    win32::PreviewPainter<CountingTarget> painter(target, counted, brushes, failure);
    painter.paint(win32::PreviewScene{*document, painted.value(), style,
                                      core::LayoutRect{0, 0, 400, 200}, 0.0F});
    expect(!failure.has_value() && target.outlines().size() == 2 && target.lines() == 2 &&
               target.text_layouts() == 4,
           "two boxes, two lines of one check and the bullet without a bracket text");
}

// 画素と色の違いの大きさ（画素は BGRA）。
[[nodiscard]] int distance_of(const std::vector<std::uint8_t> &pixels, std::int32_t x,
                              std::int32_t y, core::RgbColor color)
{
    const auto at = (static_cast<std::size_t>(y) * 400U + static_cast<std::size_t>(x)) * 4U;
    const auto channel = [](std::uint8_t value, std::uint8_t expected)
    { return value > expected ? value - expected : expected - value; };
    return channel(pixels.at(at), color.blue) + channel(pixels.at(at + 1), color.green) +
           channel(pixels.at(at + 2), color.red);
}

// 正方形の内側（枠から 2 画素）に color に近い（差 60 以下）画素があるか。
[[nodiscard]] bool tinted(const std::vector<std::uint8_t> &pixels, const core::LayoutRect &square,
                          core::RgbColor color)
{
    const auto columns = core::width_of(square) - 4;
    const auto rows = core::height_of(square) - 4;
    for (std::int32_t at = 0; at < columns * rows; ++at)
    {
        if (distance_of(pixels, square.left + 2 + (at % columns), square.top + 2 + (at / columns),
                        color) <= 60)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] core::LayoutRect shifted(const core::LayoutRect &rect, const core::LayoutRect &area)
{
    return core::LayoutRect{rect.left + area.left, rect.top + area.top, rect.right + area.left,
                            rect.bottom + area.top};
}

// 枠は muted の 1 画素の線（左右の辺の縦の中央）、accent の線はチェックした正方形の中だけ。
void expect_task_ink(const std::vector<std::uint8_t> &pixels, const core::LayoutRect &open,
                     const core::LayoutRect &done, const core::Palette &ui)
{
    expect(distance_of(pixels, open.left, (open.top + open.bottom) / 2, ui.muted) <= 6 &&
               distance_of(pixels, done.right - 1, (done.top + done.bottom) / 2, ui.muted) <= 6,
           "the box outline is drawn in the muted ink");
    expect(tinted(pixels, done, ui.accent) && !tinted(pixels, open, ui.accent),
           "only the checked box holds the accent check");
}

// 画素: 枠は muted の 1 画素の線（左右の辺の縦の中央は muted）、チェックした項目の正方形の中には
// accent の線があり、チェックしていない項目の中には無い（ADR 0155 の追補 23）。枠の hit test は
// 描いた枠の原点からの点で要素の番号を返し、押す範囲の外は nullopt（追補 24）。
void check_task_pixels()
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    const auto document = task_document();
    win32::PreviewPane pane;
    if (target == nullptr || !theme || document == nullptr ||
        !pane.prepare(*target.Get(), theme.value().view()).has_value())
    {
        expect(false, "the preview pane prepares its brushes for the task checks");
        return;
    }
    const auto style = style_of();
    const core::LayoutRect area{10, 20, 390, 180};
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    const auto painted = pane.paint(
        *target.Get(), application::PreviewFrame{document, core::Offset{0}, {}}, style, area);
    expect(SUCCEEDED(target->EndDraw()) && painted.has_value(), "the task items paint");
    PreviewTextLayouts layouts;
    const auto tops = win32::PreviewBlockTops::measure(
        *document, layouts, style,
        win32::PreviewViewport{0, static_cast<float>(core::width_of(area)),
                               static_cast<float>(core::height_of(area))});
    const auto pixels = pixels_of(*bitmap.Get());
    if (!tops || pixels.empty())
    {
        expect(false, "the task boxes are measured and the pixels are read");
        return;
    }
    const auto &open_box = tops.value().task_box(0);
    const auto &done_box = tops.value().task_box(1);
    if (!open_box.has_value() || !done_box.has_value())
    {
        expect(false, "both task items keep a check box");
        return;
    }
    expect_task_ink(pixels, shifted(open_box.value().square, area),
                    shifted(done_box.value().square, area), theme.value().view().ui);
    const auto press = shifted(done_box.value().target, area);
    const auto first = shifted(open_box.value().target, area);
    expect(pane.task_at((press.left + press.right) / 2, (press.top + press.bottom) / 2) == 1U &&
               pane.task_at(press.left, press.top) == 1U &&
               pane.task_at(first.left + 1, first.bottom - 1) == 0U,
           "a point in the press target finds the block of the box");
    expect(!pane.task_at(press.right, press.top).has_value() &&
               !pane.task_at(area.right - 5, press.top).has_value() &&
               !pane.task_at(area.left - 1, press.top).has_value(),
           "a point outside the press targets finds nothing");
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
    check_pane_picture();
    check_measured_from_anchor();
    check_list_scanned_in_range();
    check_anchor_after_carriage_returns();
    check_task_boxes();
    check_task_pixels();
}
} // namespace nenenib::tests::ui
