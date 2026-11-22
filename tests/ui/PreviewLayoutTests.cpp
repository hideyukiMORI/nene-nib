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

#include "FontSize.hpp"
#include "FullPreviewBody.hpp"
#include "Language.hpp"
#include "LayoutRect.hpp"
#include "MarkdownDocument.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "Palette.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewCodeColor.hpp"
#include "PreviewCodeColors.hpp"
#include "PreviewCodePiece.hpp"
#include "PreviewFrame.hpp"
#include "PreviewInk.hpp"
#include "PreviewMetrics.hpp"
#include "PreviewPainter.hpp"
#include "PreviewPane.hpp"
#include "PreviewPoint.hpp"
#include "PreviewScene.hpp"
#include "PreviewSelection.hpp"
#include "PreviewSelectionText.hpp"
#include "PreviewTableCell.hpp"
#include "PreviewTableMetrics.hpp"
#include "PreviewTableScroll.hpp"
#include "PreviewTaskPress.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "PreviewViewport.hpp"
#include "RenderFailure.hpp"
#include "RgbColor.hpp"
#include "Theme.hpp"
#include "ThemeCatalog.hpp"
#include "ThemeName.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <combaseapi.h>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
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
    return core::PreviewBlock{kind,
                              level,
                              0,
                              0,
                              {},
                              core::Offset{0},
                              std::move(text),
                              std::move(spans),
                              std::nullopt,
                              std::nullopt,
                              std::nullopt,
                              std::nullopt,
                              {},
                              core::PreviewCodePiece::only};
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
    expect(layouts.held() == 3 && layouts.created() == 3, "measuring holds the layouts");
    const auto again =
        layouts.get(block_of(core::PreviewBlockKind::paragraph, "text"), width, style_of());
    expect(again.has_value() && layouts.created() == 3,
           "painting a measured block reuses its layout (#1477)");
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
    expect(tops.has_value() && tops.value().size() == many &&
               layouts.held() == PreviewTextLayouts::capacity && layouts.created() == many,
           "the preview block tops hold the measured layouts up to the capacity");
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
                                      scroll, std::nullopt});
    expect(!failure.has_value() && target.balanced(), "the preview paints without failure");
    expect(layouts.created() == many && target.text_layouts() > 0 && target.text_layouts() < 10,
           "only the visible preview blocks are drawn and they reuse the measured layouts");
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
    painter.paint(win32::PreviewScene{document, tops.value(), style,
                                      core::LayoutRect{0, 0, 400, 200}, 0.0F, std::nullopt});
    expect(SUCCEEDED(target->EndDraw()) && !failure.has_value(), "the image span is painted");
    const auto layout = layouts.get(document.blocks.front(), width - 32.0F, style);
    DWRITE_FONT_STYLE slanted = DWRITE_FONT_STYLE_NORMAL;
    expect(layout.has_value() && layouts.created() == 1 &&
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
        pane.paint(*target.Get(), application::PreviewFrame{nullptr, {}, {}, {}}, style, area);
    const auto first =
        pane.paint(*target.Get(), application::PreviewFrame{shown, core::Offset{40 * 16}, {}, {}},
                   style, area);
    const auto made = pane.created();
    const auto second =
        pane.paint(*target.Get(), application::PreviewFrame{shown, core::Offset{40 * 16}, {}, {}},
                   style, area);
    expect(SUCCEEDED(target->EndDraw()) && empty.has_value() && first.has_value() &&
               second.has_value(),
           "a frame with a preview paints without failure");
    expect(made > 0 && pane.created() == made,
           "the preview layouts are reused across frames for the same document");
    expect(core::preview_anchor_block(*shown, core::Offset{40 * 16 + 3}) == 40 &&
               core::preview_anchor_block(*shown, core::Offset{5}) == 0,
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

// 同じ文書・錨 anchor・枠を PreviewPainter で別の WIC の描画先に、新しい保持と brush で直接描いた
// 画素（比べる相手）。
// 表の横の送りの量と明るいつまみの表（#1883）。
using TableScrolled = std::pair<std::vector<std::int32_t>, std::optional<std::size_t>>;
// 描く枠と錨。
using PaintPlace = std::pair<core::LayoutRect, std::size_t>;

[[nodiscard]] std::vector<std::uint8_t> painted_with(const core::MarkdownDocument &document,
                                                     const core::Theme &theme, PaintPlace place,
                                                     const TableScrolled &table)
{
    const auto &[area, anchor] = place;
    const auto &[scrolls, lit] = table;
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    PreviewTextLayouts layouts;
    win32::PreviewBrushes brushes;
    const auto style = style_of();
    const auto tops = win32::PreviewBlockTops::measure(
        document, layouts, style,
        win32::PreviewViewport{anchor, static_cast<float>(core::width_of(area)),
                               static_cast<float>(core::height_of(area))});
    if (target == nullptr || !tops || FAILED(brushes.prepare(*target.Get(), theme)))
    {
        return {};
    }
    std::optional<win32::RenderFailure> failure;
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    win32::PreviewPainter<ID2D1RenderTarget> painter(*target.Get(), layouts, brushes, failure);
    painter.paint(win32::PreviewScene{document, tops.value(), style, area, 0.0F, std::nullopt,
                                      nullptr, nullptr, scrolls, lit});
    if (FAILED(target->EndDraw()) || failure.has_value())
    {
        return {};
    }
    return pixels_of(*bitmap.Get());
}

[[nodiscard]] std::vector<std::uint8_t> directly_painted(const core::MarkdownDocument &document,
                                                         const core::Theme &theme,
                                                         const core::LayoutRect &area,
                                                         std::size_t anchor = 4)
{
    return painted_with(document, theme, PaintPlace{area, anchor}, TableScrolled{});
}

// 保持した layout は描くときに書式を変えず、位置も問わない（#1477・ADR 0155 の追補）。行内コード・
// リンク・強調の多い段落と短い段落の文書。
[[nodiscard]] std::optional<core::MarkdownDocument> repaint_document()
{
    std::string text;
    for (int word = 0; word < 40; ++word)
    {
        text += "`code` [link](x) *em* plain ";
    }
    auto parsed = core::parse_markdown(text + "\n\nsecond `one`\n", core::MarkdownStop::never());
    return parsed ? std::optional<core::MarkdownDocument>{std::move(parsed).value()} : std::nullopt;
}

// layout を作った数・SetDrawingEffect の数・HitTestTextRange の数。
[[nodiscard]] std::tuple<std::size_t, std::size_t, std::size_t>
layout_counts(const PreviewTextLayouts &layouts)
{
    return {layouts.created(), layouts.inked(), layouts.hit_tests()};
}

// 測った枠（400 × 200）に 1 回描いた画素（描く先が失敗すれば空・painter の失敗は呼び手が見る）。
[[nodiscard]] std::vector<std::uint8_t>
painted_frame(ID2D1RenderTarget &target, IWICBitmap &bitmap,
              win32::PreviewPainter<ID2D1RenderTarget> &painter, const win32::PreviewScene &scene)
{
    target.BeginDraw();
    painter.paint(scene);
    return SUCCEEDED(target.EndDraw()) ? pixels_of(bitmap) : std::vector<std::uint8_t>{};
}

// 最初の要素の layout の行内コード（UTF-16 の 1）に brushes の string の色が置いてあるか。
[[nodiscard]] bool code_inked(PreviewTextLayouts &layouts, const win32::PreviewScene &scene,
                              const win32::PreviewBrushes &brushes)
{
    const auto &block = scene.document.blocks.front();
    const auto layout = layouts.get(
        block, win32::preview_text_width(block, 400.0F, scene.tops.indent(0), scene.style.dpi),
        scene.style);
    return layout.has_value() &&
           effect_is(*layout.value().Get(), 1, brushes.of(win32::PreviewInk::string));
}

// 2 回描くと 2 回目は layout を作らず SetDrawingEffect も HitTestTextRange も呼ばず、
// 画素は 1 回目と同じ。同じ描画先でテーマを変えると brush の色だけを置き直し（#1485）、
// 保持した layout は作り直さず色も面の矩形も引き直さない。画素は新しいテーマを新しい
// 保持で描いたものと 1 画素も違わない。
void check_repaint_keeps_layouts_clean()
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    const auto other = catalog.find(core::ThemeName::parse("solarized-light").value());
    win32::PreviewBrushes brushes;
    const auto document = repaint_document();
    if (!document.has_value())
    {
        expect(false, "the repaint document parses");
        return;
    }
    PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto tops = win32::PreviewBlockTops::measure(document.value(), layouts, style,
                                                       win32::PreviewViewport{0, 400.0F, 200.0F});
    if (target == nullptr || !theme || !other || !tops ||
        FAILED(brushes.prepare(*target.Get(), theme.value().view())))
    {
        expect(false, "the repaint checks prepare a target, brushes and a document");
        return;
    }
    std::optional<win32::RenderFailure> failure;
    win32::PreviewPainter<ID2D1RenderTarget> painter(*target.Get(), layouts, brushes, failure);
    const win32::PreviewScene scene{
        document.value(), tops.value(), style, core::LayoutRect{0, 0, 400, 200}, 0.0F,
        std::nullopt};
    const auto measured = layout_counts(layouts);
    const auto first = painted_frame(*target.Get(), *bitmap.Get(), painter, scene);
    const auto painted = layout_counts(layouts);
    const auto second = painted_frame(*target.Get(), *bitmap.Get(), painter, scene);
    expect(!first.empty() && !failure.has_value() &&
               std::get<0>(painted) == std::get<0>(measured) &&
               std::get<1>(painted) > std::get<1>(measured) &&
               std::get<2>(painted) == std::get<2>(measured),
           "the first paint reuses the measured layouts and inks them once");
    expect(layout_counts(layouts) == painted,
           "a repaint creates no layout, sets no drawing effect and hit-tests nothing");
    expect(!second.empty() && second == first && code_inked(layouts, scene, brushes),
           "a repaint has the same pixels and the inline code keeps the string ink");
    const auto generation = brushes.generation();
    expect(SUCCEEDED(brushes.prepare(*target.Get(), other.value().view())) &&
               brushes.generation() == generation,
           "another theme on the same target recolours the same brushes");
    const auto recolored = painted_frame(*target.Get(), *bitmap.Get(), painter, scene);
    expect(!recolored.empty() && recolored != first && layout_counts(layouts) == painted &&
               code_inked(layouts, scene, brushes),
           "another theme keeps the layouts, their ink and their inline code rectangles");
    expect(recolored == directly_painted(document.value(), other.value().view(),
                                         core::LayoutRect{0, 0, 400, 200}, 0),
           "the recoloured pixels equal painting the other theme with new layouts and brushes");
}

// 同じ PreviewBrushes を別の描く先で prepare すると brush を作り直して generation が
// 1 進み、bind が色を置いた保持を捨てる（古い描く先の brush を指す layout を残さない・
// ADR 0155 の決定 71・73）。次の描画は layout を作り直して色を置き直し、画素は 2 つ目の
// 描く先に新しい保持と brush で直接描いたものと 1 画素も違わない。
void check_target_change_drops_inked()
{
    Microsoft::WRL::ComPtr<IWICBitmap> first_bitmap;
    Microsoft::WRL::ComPtr<IWICBitmap> second_bitmap;
    const auto first_target = wic_target(first_bitmap);
    const auto second_target = wic_target(second_bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    const auto document = repaint_document();
    win32::PreviewBrushes brushes;
    PreviewTextLayouts layouts;
    const auto style = style_of();
    if (!document.has_value())
    {
        expect(false, "the document for the brush checks parses");
        return;
    }
    const auto tops = win32::PreviewBlockTops::measure(document.value(), layouts, style,
                                                       win32::PreviewViewport{0, 400.0F, 200.0F});
    if (first_target == nullptr || second_target == nullptr || !theme || !tops ||
        FAILED(brushes.prepare(*first_target.Get(), theme.value().view())))
    {
        expect(false, "the target change checks prepare two targets, brushes and a document");
        return;
    }
    const win32::PreviewScene scene{
        document.value(), tops.value(), style, core::LayoutRect{0, 0, 400, 200}, 0.0F,
        std::nullopt};
    std::optional<win32::RenderFailure> failure;
    win32::PreviewPainter<ID2D1RenderTarget> first(*first_target.Get(), layouts, brushes, failure);
    const auto before = painted_frame(*first_target.Get(), *first_bitmap.Get(), first, scene);
    const auto inked = layout_counts(layouts);
    const auto generation = brushes.generation();
    expect(!before.empty() && !failure.has_value() &&
               SUCCEEDED(brushes.prepare(*second_target.Get(), theme.value().view())) &&
               brushes.generation() == generation + 1,
           "another target makes the brushes again and advances the generation");
    win32::PreviewPainter<ID2D1RenderTarget> second(*second_target.Get(), layouts, brushes,
                                                    failure);
    const auto after = painted_frame(*second_target.Get(), *second_bitmap.Get(), second, scene);
    expect(!after.empty() && !failure.has_value() &&
               std::get<0>(layout_counts(layouts)) > std::get<0>(inked) &&
               std::get<1>(layout_counts(layouts)) > std::get<1>(inked) &&
               code_inked(layouts, scene, brushes),
           "another target drops the layouts inked with the old brushes and inks new ones");
    expect(after == directly_painted(document.value(), theme.value().view(),
                                     core::LayoutRect{0, 0, 400, 200}, 0),
           "after a target change the pixels equal painting with new layouts and brushes");
}

// 色の役割を 1 つの枠に集めた文書（見出し・行内コード・リンク・画像の代わりの字・タスクの箱の
// 済みと未・番号・引用・コードブロック・水平線）。選択の面（selection）だけは入らない。
[[nodiscard]] std::optional<core::MarkdownDocument> inks_document()
{
    auto parsed = core::parse_markdown("# Head\n\n`code` [link](x) ![alt](y.png) plain\n\n"
                                       "- [x] done\n- [ ] open\n\n1. one\n\n> quoted\n\n"
                                       "```\nint x;\n```\n\n---\n",
                                       core::MarkdownStop::never());
    return parsed ? std::optional<core::MarkdownDocument>{std::move(parsed).value()} : std::nullopt;
}

// 同じ描く先でテーマを変えた後の画素は、選択の面を除く全部の役割で、新しいテーマを新しい保持と
// brush で描いたものと 1 画素も違わない（#1485・brush の色の置き直し）。
void check_recolour_every_ink()
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    const auto other = catalog.find(core::ThemeName::parse("solarized-light").value());
    const auto document = inks_document();
    win32::PreviewBrushes brushes;
    PreviewTextLayouts layouts;
    const auto style = style_of();
    if (!document.has_value())
    {
        expect(false, "the document for the brush checks parses");
        return;
    }
    const auto tops = win32::PreviewBlockTops::measure(document.value(), layouts, style,
                                                       win32::PreviewViewport{0, 400.0F, 200.0F});
    if (target == nullptr || !theme || !other || !tops ||
        FAILED(brushes.prepare(*target.Get(), theme.value().view())))
    {
        expect(false, "the recolour checks prepare a target, brushes and a document");
        return;
    }
    const win32::PreviewScene scene{
        document.value(), tops.value(), style, core::LayoutRect{0, 0, 400, 200}, 0.0F,
        std::nullopt};
    std::optional<win32::RenderFailure> failure;
    win32::PreviewPainter<ID2D1RenderTarget> painter(*target.Get(), layouts, brushes, failure);
    const auto first = painted_frame(*target.Get(), *bitmap.Get(), painter, scene);
    const auto recoloured =
        !first.empty() && SUCCEEDED(brushes.prepare(*target.Get(), other.value().view()))
            ? painted_frame(*target.Get(), *bitmap.Get(), painter, scene)
            : std::vector<std::uint8_t>{};
    expect(!recoloured.empty() && !failure.has_value() && recoloured != first &&
               recoloured == directly_painted(document.value(), other.value().view(),
                                              core::LayoutRect{0, 0, 400, 200}, 0),
           "a recoloured frame of every ink but the selection equals painting the other theme");
}

// 決定的な数の列（線形合同・試験の入力を作るだけ）。
[[nodiscard]] std::uint32_t next_number(std::uint32_t &state) noexcept
{
    constexpr std::uint32_t multiplier = 1664525U;
    constexpr std::uint32_t increment = 1013904223U;
    state = state * multiplier + increment;
    return state >> 8U;
}

// 1 つの文字列について、範囲の列を preview_unit_ranges で写した値が、範囲ごとに先頭から数える
// preview_units と同じか。
[[nodiscard]] bool same_units(std::string_view shown,
                              std::span<const win32::PreviewByteRange> ranges)
{
    const auto placed = win32::preview_unit_ranges(shown, ranges);
    if (placed.size() != ranges.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < ranges.size(); ++index)
    {
        const auto from = win32::preview_units(shown, ranges[index].first);
        const auto to = win32::preview_units(shown, ranges[index].second);
        if (placed.at(index).startPosition != from ||
            placed.at(index).length != (to > from ? to - from : 0U))
        {
            return false;
        }
    }
    return true;
}

// preview_unit_ranges（#1485）は範囲ごとに先頭から数える preview_units と同じ値になる。固定の表
// （ASCII・2/3/4 バイト・結合文字・空の範囲・逆の範囲・重なり・長さを超える端・code point の途中の
// 端・正しくない UTF-8）と、固定の seed で作った文字と範囲の列で比べる。
void check_unit_ranges_match_units()
{
    // é（2 バイト）・日（3 バイト）・😀（4 バイト・サロゲートペア）・e＋結合のアキュート。
    const std::string mixed = "a\xC3\xA9\xE6\x97\xA5\xF0\x9F\x98\x80"
                              "e\xCC\x81z";
    const std::vector<win32::PreviewByteRange> table{
        {0, 1}, {1, 3},  {3, 6},  {6, 10},  {10, 13}, {13, 14}, {0, 14}, {5, 5},  {9, 4},
        {2, 8}, {4, 12}, {0, 40}, {30, 50}, {2, 2},   {7, 8},   {1, 13}, {14, 14}};
    expect(same_units(mixed, table), "the fixed table maps to the same units as preview_units");
    const std::string broken = "ab\xFF"
                               "cd\xE6\x97";
    expect(same_units(broken, std::vector<win32::PreviewByteRange>{{0, 2}, {1, 4}, {3, 7}, {0, 6}}),
           "invalid UTF-8 maps to the same units as preview_units");
    expect(same_units("", std::vector<win32::PreviewByteRange>{{0, 0}, {0, 3}}) &&
               same_units(mixed, std::vector<win32::PreviewByteRange>{}),
           "empty text and no ranges map to the same units as preview_units");
    constexpr std::array<std::string_view, 6> pieces{
        "a", " ", "\xC3\xA9", "\xE6\x97\xA5", "\xF0\x9F\x98\x80", "e\xCC\x81"};
    std::uint32_t state = 1485U;
    bool same = true;
    for (int round = 0; round < 200 && same; ++round)
    {
        std::string text;
        const auto count = next_number(state) % 40U;
        for (std::uint32_t at = 0; at < count; ++at)
        {
            text += pieces.at(next_number(state) % pieces.size());
        }
        std::vector<win32::PreviewByteRange> ranges;
        const auto limit = text.size() + 3U;
        const auto spans = next_number(state) % 12U;
        for (std::uint32_t at = 0; at < spans; ++at)
        {
            ranges.emplace_back(next_number(state) % limit, next_number(state) % limit);
        }
        same = same_units(text, ranges);
    }
    expect(same, "200 seeded texts and ranges map to the same units as preview_units");
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
    const application::PreviewFrame at_four{document, core::Offset{4 * 16}, {}, {}};
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
    const auto moved =
        pane.paint(*held.Get(), application::PreviewFrame{document, core::Offset{8 * 16}, {}, {}},
                   style, area);
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

// 全面のプレビュー（ADR 0155 の改訂 15・#1145）は 1 枠の本文の帯の全部（gutter の列を含む）に描く。
// 前の frame の本文の色（ここでは全体を塗った印の色）は帯のどこにも残らない＝gutter も本文も
// 描かれない。帯は面を除いた client の左端から始まる。
void check_full_preview_band()
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    win32::PreviewPane pane;
    if (target == nullptr || !theme ||
        !pane.prepare(*target.Get(), theme.value().view()).has_value())
    {
        expect(false, "the preview pane prepares its brushes for the full band check");
        return;
    }
    const core::LayoutRect area{0, 0, 400, 200};
    const auto body = win32::full_preview_body(area, 96, core::default_font_size());
    expect(body.band.left == area.left && body.band.right == area.right &&
               body.gutter.left >= body.band.left && body.gutter.right > body.gutter.left,
           "the full preview band spans the gutter and the text of one pane");
    target->BeginDraw();
    target->Clear(D2D1::ColorF(1.0F, 0.0F, 1.0F, 1.0F));
    const auto painted = pane.paint(
        *target.Get(), application::PreviewFrame{picture_document(), core::Offset{0}, {}, {}},
        style_of(), body.band);
    expect(SUCCEEDED(target->EndDraw()) && painted.has_value(),
           "the full preview paints without failure");
    const auto pixels = pixels_of(*bitmap.Get());
    bool marked = pixels.empty();
    for (auto y = body.band.top; y < body.band.bottom && !marked; ++y)
    {
        for (auto x = body.band.left; x < body.band.right && !marked; ++x)
        {
            const auto at = (static_cast<std::size_t>(y) * 400U + static_cast<std::size_t>(x)) * 4U;
            marked =
                pixels.at(at) == 0xFF && pixels.at(at + 1U) == 0x00 && pixels.at(at + 2U) == 0xFF;
        }
    }
    expect(!marked, "nothing of the body or the gutter shows under the full preview");
}

// 全面プレビューの選択（ADR 0155 の改訂 18）。選択の面は文字の下に selection の色で塗り、空の選択は
// 何も変えず、選んでいない要素の画素は変わらない。点の下の文字の位置は描いた layout の hit test
// で、 文字の左は要素の頭・行の右の外は要素の終わり・要素の間は上の要素の終わり・最後の要素より下は
// その終わり。
[[nodiscard]] std::vector<std::uint8_t> selection_painted(ID2D1RenderTarget &target,
                                                          IWICBitmap &bitmap,
                                                          win32::PreviewPane &pane,
                                                          const application::PreviewFrame &frame)
{
    target.BeginDraw();
    target.Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    const auto painted = pane.paint(target, frame, style_of(), core::LayoutRect{0, 0, 400, 200});
    if (FAILED(target.EndDraw()) || !painted.has_value())
    {
        return {};
    }
    return pixels_of(bitmap);
}

[[nodiscard]] bool same_rows(const std::vector<std::uint8_t> &left,
                             const std::vector<std::uint8_t> &right, std::int32_t top,
                             std::int32_t bottom)
{
    constexpr std::size_t stride = 400U * 4U;
    const auto begin = static_cast<std::size_t>(std::max(top, 0)) * stride;
    const auto end = std::min(static_cast<std::size_t>(std::max(bottom, 0)) * stride, left.size());
    return left.size() == right.size() &&
           std::equal(left.begin() + static_cast<std::ptrdiff_t>(begin),
                      left.begin() + static_cast<std::ptrdiff_t>(end),
                      right.begin() + static_cast<std::ptrdiff_t>(begin));
}

[[nodiscard]] std::shared_ptr<const core::MarkdownDocument> selection_document()
{
    auto parsed =
        core::parse_markdown("first paragraph\n\nsecond one", core::MarkdownStop::never());
    if (!parsed)
    {
        return nullptr;
    }
    return std::make_shared<const core::MarkdownDocument>(std::move(parsed).value());
}

// 点の下の文字の位置（描いた後の pane の hit test）。
void check_selection_points(win32::PreviewPane &pane, const win32::PreviewBlockTops &measured)
{
    const auto first_top = static_cast<std::int32_t>(measured.top(0));
    const auto first_bottom = static_cast<std::int32_t>(measured.bottom(0));
    const auto second_top = static_cast<std::int32_t>(measured.top(1));
    const auto left = static_cast<std::int32_t>(measured.indent(0));
    const auto middle = (first_top + first_bottom) / 2;
    const auto first_end = std::string_view("first paragraph").size();
    const auto gap = pane.point_at(left, (first_bottom + second_top) / 2);
    const auto inside = pane.point_at(left + 2, middle);
    expect(pane.point_at(left - 4, middle) == core::PreviewPoint{0, 0} &&
               pane.point_at(399, middle) == core::PreviewPoint{0, first_end} &&
               (second_top <= first_bottom || gap == core::PreviewPoint{0, first_end}) &&
               pane.point_at(left, 199) ==
                   core::PreviewPoint{1, std::string_view("second one").size()} &&
               inside.has_value() && inside.value_or(core::PreviewPoint{9, 9}).block == 0 &&
               inside.value_or(core::PreviewPoint{9, 9}).offset <= 1U,
           "the point under the pointer is the character of the drawn layout");
}

void check_selection_pixels()
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    const auto document = selection_document();
    win32::PreviewPane pane;
    if (target == nullptr || !theme || document == nullptr ||
        !pane.prepare(*target.Get(), theme.value().view()).has_value())
    {
        expect(false, "the preview pane prepares its brushes for the selection checks");
        return;
    }
    const auto frame_of = [&document](std::optional<core::PreviewSelection> selection)
    { return application::PreviewFrame{document, core::Offset{0}, {}, selection}; };
    const auto plain =
        selection_painted(*target.Get(), *bitmap.Get(), pane, frame_of(std::nullopt));
    const auto empty = selection_painted(*target.Get(), *bitmap.Get(), pane,
                                         frame_of(core::PreviewSelection{0, 3, 0, 3}));
    const auto chosen = selection_painted(*target.Get(), *bitmap.Get(), pane,
                                          frame_of(core::PreviewSelection{0, 0, 0, 5}));
    PreviewTextLayouts layouts;
    const auto tops = win32::PreviewBlockTops::measure(*document, layouts, style_of(),
                                                       win32::PreviewViewport{0, 400.0F, 200.0F});
    if (plain.empty() || chosen.empty() || !tops || tops.value().size() != 2)
    {
        expect(false, "the selection paints and both blocks are measured");
        return;
    }
    const auto &measured = tops.value();
    expect(plain == empty, "an empty preview selection paints nothing");
    expect(!same_rows(plain, chosen, static_cast<std::int32_t>(measured.top(0)),
                      static_cast<std::int32_t>(measured.bottom(0))) &&
               same_rows(plain, chosen, static_cast<std::int32_t>(measured.top(1)),
                         static_cast<std::int32_t>(measured.bottom(1))),
           "the selection face shows under the chosen text only");
    check_selection_points(pane, measured);
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
               core::preview_anchor_block(parsed.value(), core::Offset{3}) == 0 &&
               core::preview_anchor_block(parsed.value(), core::Offset{6}) == 1 &&
               core::preview_anchor_block(parsed.value(), core::Offset{11}) == 2,
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
                                      core::LayoutRect{0, 0, 400, 200}, 0.0F, std::nullopt});
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
        *target.Get(), application::PreviewFrame{document, core::Offset{0}, {}, {}}, style, area);
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
// 押した要素と離した要素が同じときだけ切り替える（ADR 0056 の決定 9・#1102 のレビュー
// R1-1）。窓の外で 離して WM_LBUTTONUP が来なくても、次の WM_LBUTTONDOWN の入口の forget
// で押しは消え、別の所を 押してから同じ箱の上で離しても切り替わらない。
void check_task_press()
{
    win32::PreviewTaskPress press;
    press.press(1U);
    expect(press.release(1U) == 1U, "pressing and releasing on the same box toggles it");
    expect(!press.release(1U).has_value(), "a release without a press toggles nothing");
    press.press(1U);
    expect(!press.release(0U).has_value() && !press.pressed(),
           "releasing on another box toggles nothing and forgets the press");
    press.press(1U);
    press.forget();
    press.press(std::nullopt);
    expect(!press.release(1U).has_value(),
           "a press elsewhere after a lost release does not toggle the box released on");
    press.press(1U);
    press.forget();
    expect(!press.release(1U).has_value(), "the next press forgets a press left over");
}

// 画素の行のうち、地の色（枠の左上の画素）と違う画素が横に続くいちばん長い数。
[[nodiscard]] std::int32_t longest_inked_run(const std::vector<std::uint8_t> &pixels)
{
    const auto ground = [&pixels](std::size_t at)
    {
        return std::equal(pixels.begin() + static_cast<std::ptrdiff_t>(at),
                          pixels.begin() + static_cast<std::ptrdiff_t>(at) + 3, pixels.begin());
    };
    std::int32_t longest = 0;
    for (std::size_t row = 0; row < 200U; ++row)
    {
        std::int32_t run = 0;
        for (std::size_t column = 0; column < 400U; ++column)
        {
            run = ground((row * 400U + column) * 4U) ? 0 : run + 1;
            longest = std::max(longest, run);
        }
    }
    return longest;
}

// 打ち消し線（#1840）: 間の空いた字の列に打ち消しを掛けると、字の間の空白をまたいで横に続く線が
// 描かれる（線の太さと位置は DirectWrite に任せる）。掛けない同じ文字には無い。保持の鍵は装飾を
// 含むので、同じ文字の打ち消しの有無は別の layout。
void check_strikethrough_pixels()
{
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    if (!theme)
    {
        expect(false, "the theme for the strike through check is found");
        return;
    }
    const std::string text = "i      i      i      i";
    core::MarkdownDocument plain;
    plain.blocks.push_back(block_of(core::PreviewBlockKind::paragraph, text));
    core::MarkdownDocument struck;
    struck.blocks.push_back(block_of(
        core::PreviewBlockKind::paragraph, text,
        {{core::Offset{0}, core::Offset{text.size()}, core::PreviewStyle::strikethrough}}));
    const core::LayoutRect area{0, 0, 400, 200};
    const auto plain_pixels = directly_painted(plain, theme.value().view(), area, 0);
    const auto struck_pixels = directly_painted(struck, theme.value().view(), area, 0);
    if (plain_pixels.empty() || struck_pixels.empty())
    {
        expect(false, "both strike through samples paint");
        return;
    }
    // 字 i の幅は数画素。線は字の間の空白（6 文字ぶん）をまたいで 60 画素を超えて続く。
    expect(longest_inked_run(plain_pixels) < 20 && longest_inked_run(struck_pixels) > 60,
           "a strike through draws one line across the spaces between the letters");
    PreviewTextLayouts layouts;
    const auto bare = layouts.get(plain.blocks.front(), width, style_of());
    const auto lined = layouts.get(struck.blocks.front(), width, style_of());
    BOOL has_line = FALSE;
    expect(bare.has_value() && lined.has_value() && layouts.created() == 2 &&
               SUCCEEDED(lined.value()->GetStrikethrough(3, &has_line)) && has_line != FALSE,
           "the strike through is a separate held layout with the DirectWrite strike through");
}

// front matter（#1840）: 囲みのコードと同じ panel の面（内側の余白・等幅・
// 折らない）に comment の色で描く。同じ文字のコードブロックとは文字の色だけが違う。
void check_front_matter_pixels()
{
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    if (!theme)
    {
        expect(false, "the theme for the front matter check is found");
        return;
    }
    core::MarkdownDocument fronted;
    fronted.blocks.push_back(block_of(core::PreviewBlockKind::front_matter, "title: a\n"));
    core::MarkdownDocument coded;
    coded.blocks.push_back(block_of(core::PreviewBlockKind::code, "title: a\n"));
    const core::LayoutRect area{0, 0, 400, 200};
    const auto front_pixels = directly_painted(fronted, theme.value().view(), area, 0);
    const auto code_pixels = directly_painted(coded, theme.value().view(), area, 0);
    if (front_pixels.empty() || code_pixels.empty())
    {
        expect(false, "both front matter samples paint");
        return;
    }
    const auto &palette = theme.value().view();
    // 面の右寄り（文字の無い所・角丸の外れ）の画素は panel。面の下の地は background。
    expect(distance_of(front_pixels, 360, 22, palette.ui.panel) <= 6 &&
               distance_of(front_pixels, 360, 190, palette.body.background) <= 6,
           "the front matter is painted on the panel like a code block");
    expect(distance_of(code_pixels, 360, 22, palette.ui.panel) <= 6 && front_pixels != code_pixels,
           "the front matter differs from the same code block only in its text ink");
    PreviewTextLayouts layouts;
    const auto layout = layouts.get(fronted.blocks.front(), width, style_of());
    std::array<wchar_t, 64> family{};
    expect(layout.has_value() &&
               layout.value()->GetWordWrapping() == DWRITE_WORD_WRAPPING_NO_WRAP &&
               SUCCEEDED(layout.value()->GetFontFamilyName(0, family.data(),
                                                           static_cast<UINT32>(family.size()))) &&
               std::wstring_view(family.data()) != L"Segoe UI",
           "the front matter is monospaced and does not wrap");
}

// 枠の左上を原点にした画素の region の中で、地の色と違う画素のいちばん右の x（無ければ -1）。
[[nodiscard]] bool inked_at(const std::vector<std::uint8_t> &pixels, std::int32_t x, std::int32_t y)
{
    const auto at = (static_cast<std::size_t>(y) * 400U + static_cast<std::size_t>(x)) * 4U;
    return !std::equal(pixels.begin() + static_cast<std::ptrdiff_t>(at),
                       pixels.begin() + static_cast<std::ptrdiff_t>(at) + 3, pixels.begin());
}

[[nodiscard]] std::int32_t rightmost_ink(const std::vector<std::uint8_t> &pixels,
                                         const core::LayoutRect &region)
{
    std::int32_t right = -1;
    for (std::int32_t at = 0; at < core::width_of(region) * core::height_of(region); ++at)
    {
        const auto x = region.left + (at % core::width_of(region));
        if (inked_at(pixels, x, region.top + (at / core::width_of(region))))
        {
            right = std::max(right, x);
        }
    }
    return right;
}

// 表（#1848・採用案 D103 の案 B）: 見出しの行の下に panel_border の 2 DIP の罫、続く行の間に 1 DIP
// の罫（外枠と縦の罫は無い）。右揃えの列は升の文字の右端が揃う。行の高さは行高 ＋ 上下 7 DIP。
void check_table_pixels()
{
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    const auto parsed = core::parse_markdown("| head | n |\n|---|--:|\n| a | 1 |\n| b | 22222 |\n",
                                             core::MarkdownStop::never());
    if (!theme || !parsed || parsed.value().blocks.size() != 3U)
    {
        expect(false, "the table sample parses into three rows");
        return;
    }
    const auto &document = parsed.value();
    const auto style = style_of();
    const core::LayoutRect area{0, 0, 400, 200};
    const auto pixels = directly_painted(document, theme.value().view(), area, 0);
    PreviewTextLayouts layouts;
    const auto tops = win32::PreviewBlockTops::measure(document, layouts, style,
                                                       win32::PreviewViewport{0, 400.0F, 200.0F});
    const auto columns = layouts.table_columns(document, 0, style);
    if (pixels.empty() || !tops || !columns)
    {
        expect(false, "the table paints and is measured");
        return;
    }
    const auto &palette = theme.value().view();
    const auto &at = tops.value();
    const auto line = win32::preview_line_pixels(style);
    expect(at.bottom(0) - at.top(0) == line + 14.0F && at.top(1) == at.bottom(0) &&
               at.top(2) == at.bottom(1),
           "a table row is the line height and 7 DIP above and below, with no gap between rows");
    const auto left = static_cast<std::int32_t>(at.indent(0)) + 2;
    const auto header_bottom = static_cast<std::int32_t>(at.bottom(0));
    const auto row_top = static_cast<std::int32_t>(at.top(2));
    expect(distance_of(pixels, left, header_bottom - 1, palette.ui.panel_border) <= 6 &&
               distance_of(pixels, left, header_bottom - 2, palette.ui.panel_border) <= 6 &&
               distance_of(pixels, left, header_bottom - 3, palette.body.background) <= 6,
           "the header row has a 2 DIP rule in panel_border under it");
    expect(distance_of(pixels, left, row_top, palette.ui.panel_border) <= 6 &&
               distance_of(pixels, left, row_top + 1, palette.body.background) <= 6,
           "a body row after a body row has a 1 DIP rule above it");
    const auto table_right =
        static_cast<std::int32_t>(at.indent(0) + win32::preview_table_width(*columns.value(), 96));
    expect(distance_of(pixels, table_right + 3, header_bottom - 1, palette.body.background) <= 6 &&
               distance_of(pixels, left, static_cast<std::int32_t>(at.top(0)) + 1,
                           palette.body.background) <= 6,
           "the rules end at the table width and there is no outer frame");
    const auto second = static_cast<std::int32_t>(
        at.indent(0) + win32::preview_table_cell_left(*columns.value(), 1, 0.0F, 96));
    const auto band = [&at, second](std::size_t row)
    {
        return core::LayoutRect{second, static_cast<std::int32_t>(at.top(row)) + 2, 399,
                                static_cast<std::int32_t>(at.bottom(row)) - 2};
    };
    const auto first_right = rightmost_ink(pixels, band(1));
    const auto second_right = rightmost_ink(pixels, band(2));
    expect(first_right > second && second_right > second && first_right - second_right <= 2 &&
               second_right - first_right <= 2,
           "the cells of a right aligned column end at the same right edge");
}

// alert（#1848・D103）: 左の線は 3 DIP で種類の色（WARNING は warning）、ふつうの引用は 2 DIP の
// comment。種類の名前は太字。
void check_alert_pixels()
{
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    const auto alert = core::parse_markdown("> [!WARNING]\n> body\n", core::MarkdownStop::never());
    const auto plain = core::parse_markdown("> body\n", core::MarkdownStop::never());
    if (!theme || !alert || !plain)
    {
        expect(false, "the alert samples parse");
        return;
    }
    const auto &palette = theme.value().view();
    const core::LayoutRect area{0, 0, 400, 200};
    const auto alert_pixels = directly_painted(alert.value(), palette, area, 0);
    const auto plain_pixels = directly_painted(plain.value(), palette, area, 0);
    if (alert_pixels.empty() || plain_pixels.empty())
    {
        expect(false, "both alert samples paint");
        return;
    }
    const auto x = static_cast<std::int32_t>(win32::preview_quote_left(0, 96));
    constexpr std::int32_t y = 20;
    expect(distance_of(alert_pixels, x, y, palette.body.warning) <= 6 &&
               distance_of(alert_pixels, x + 2, y, palette.body.warning) <= 6 &&
               distance_of(alert_pixels, x + 3, y, palette.body.background) <= 6,
           "the alert rule is 3 DIP in the warning ink");
    expect(distance_of(plain_pixels, x, y, palette.body.comment) <= 6 &&
               distance_of(plain_pixels, x + 2, y, palette.body.background) <= 6,
           "an ordinary quote keeps the 2 DIP comment rule");
    PreviewTextLayouts layouts;
    const auto layout = layouts.get(alert.value().blocks.front(), width, style_of());
    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
    DWRITE_FONT_WEIGHT body = DWRITE_FONT_WEIGHT_BOLD;
    expect(layout.has_value() && SUCCEEDED(layout.value()->GetFontWeight(1, &weight)) &&
               SUCCEEDED(layout.value()->GetFontWeight(9, &body)) &&
               weight == DWRITE_FONT_WEIGHT_BOLD && body == DWRITE_FONT_WEIGHT_NORMAL,
           "the kind name is bold and the body is not");
}

// 大きな表（1,000 行 × 8 列）の費用（#1848）: 列の幅は表ごとに列の数だけの一時の文字組みで 1 回
// 測り、升の文字組みは見えている行の分だけ作る。表の前の段落を打ち替えた文書を描き直すと、表は
// 測り直さず、作るのは打ち替えた段落の 1 つだけ。
[[nodiscard]] std::string large_table_text()
{
    std::string text = "lead\n\n| h0 | h1 | h2 | h3 | h4 | h5 | h6 | h7 |\n"
                       "|---|---|---|---|---|---|---|---|\n";
    for (int cell = 0; cell < 8000; ++cell)
    {
        text +=
            (cell % 8 == 0 ? "| " : " ") + std::to_string(cell) + (cell % 8 == 7 ? " |\n" : " |");
    }
    return text;
}

void check_large_table_cost()
{
    const auto text = large_table_text();
    const auto before = core::parse_markdown(text, core::MarkdownStop::never());
    const auto after = core::parse_markdown("lead!" + text.substr(4), core::MarkdownStop::never());
    if (!before || !after || before.value().blocks.size() != 1002U)
    {
        expect(false, "the large table parses into a paragraph and 1,001 rows");
        return;
    }
    const auto style = style_of();
    PreviewTextLayouts layouts;
    const auto paint = [&](const core::MarkdownDocument &document)
    {
        const auto tops = win32::PreviewBlockTops::measure(
            document, layouts, style, win32::PreviewViewport{0, width, 200.0F});
        if (!tops)
        {
            return false;
        }
        CountingTarget target;
        const win32::PreviewBrushes brushes;
        std::optional<win32::RenderFailure> failure;
        win32::PreviewPainter<CountingTarget> painter(target, layouts, brushes, failure);
        painter.paint(win32::PreviewScene{
            document, tops.value(), style,
            core::LayoutRect{0, 0, static_cast<std::int32_t>(width), 200}, 0.0F, std::nullopt});
        return !failure.has_value() && target.balanced();
    };
    const bool first = paint(before.value());
    const auto made = layouts.created();
    const auto measured = layouts.measured_tables();
    const bool second = paint(after.value());
    expect(
        first && second && measured == 1U && layouts.measured_tables() == 1U,
        "the column widths of a large table are measured once and kept across an edit before it");
    expect(made <= 1U + 8U + 8U * 6U && layouts.created() - made == 1U,
           "only the visible cells are made and an edit before the table makes one layout");
}
// 小さな表が 6 つ見えている文書（#1852 のレビュー）: 2 回目の描画では列の幅を測り直さず、文字組み
// も作らない（覚えの上限が見えている表の数より多い）。
void check_many_tables_kept()
{
    std::string text;
    for (int table = 0; table < 6; ++table)
    {
        text += "| t" + std::to_string(table) + " |\n|---|\n\n";
    }
    const auto parsed = core::parse_markdown(text, core::MarkdownStop::never());
    if (!parsed || parsed.value().blocks.size() != 6U)
    {
        expect(false, "six one row tables parse into six rows");
        return;
    }
    const auto style = style_of();
    PreviewTextLayouts layouts;
    const auto paint = [&]
    {
        const auto tops = win32::PreviewBlockTops::measure(
            parsed.value(), layouts, style, win32::PreviewViewport{0, width, 400.0F});
        CountingTarget target;
        const win32::PreviewBrushes brushes;
        std::optional<win32::RenderFailure> failure;
        win32::PreviewPainter<CountingTarget> painter(target, layouts, brushes, failure);
        if (tops)
        {
            painter.paint(win32::PreviewScene{
                parsed.value(), tops.value(), style,
                core::LayoutRect{0, 0, static_cast<std::int32_t>(width), 400}, 0.0F, std::nullopt});
        }
        return tops.has_value() && !failure.has_value() && target.balanced();
    };
    const bool first = paint();
    const auto measured = layouts.measured_tables();
    const auto made = layouts.created();
    const bool second = paint();
    expect(first && second && measured == 6U && layouts.measured_tables() == measured &&
               layouts.created() == made,
           "six visible tables are measured once and a second paint measures and makes nothing");
}

// 要素 index の layout の先頭の文字に brush の色が置いてあるか。
[[nodiscard]] bool block_inked(PreviewTextLayouts &layouts, const win32::PreviewScene &scene,
                               std::size_t index, ID2D1Brush *brush)
{
    const auto &block = scene.document.blocks.at(index);
    const auto layout = layouts.get(
        block, win32::preview_text_width(block, 400.0F, scene.tops.indent(index), scene.style.dpi),
        scene.style);
    return layout.has_value() && effect_is(*layout.value().Get(), 0U, brush);
}

// 文書（大きな囲み 1 つ）を 400 × 200 に描き、最初の描画で置いた色の数が inks で、描き直しと
// 送りで layout の生成も色の置き直しも 0。
void check_fence_paint(const core::MarkdownDocument &document, std::size_t inks)
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    win32::PreviewBrushes brushes;
    PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto tops = win32::PreviewBlockTops::measure(document, layouts, style,
                                                       win32::PreviewViewport{0, 400.0F, 200.0F});
    if (target == nullptr || !theme || !tops ||
        FAILED(brushes.prepare(*target.Get(), theme.value().view())))
    {
        expect(false, "the large fence check prepares a target and brushes");
        return;
    }
    std::optional<win32::RenderFailure> failure;
    win32::PreviewPainter<ID2D1RenderTarget> painter(*target.Get(), layouts, brushes, failure);
    const auto scene_at = [&](float scroll)
    {
        return win32::PreviewScene{document, tops.value(), style, core::LayoutRect{0, 0, 400, 200},
                                   scroll,   std::nullopt};
    };
    static_cast<void>(painted_frame(*target.Get(), *bitmap.Get(), painter, scene_at(0.0F)));
    const auto painted = layout_counts(layouts);
    static_cast<void>(painted_frame(*target.Get(), *bitmap.Get(), painter, scene_at(0.0F)));
    static_cast<void>(painted_frame(*target.Get(), *bitmap.Get(), painter, scene_at(4000.0F)));
    expect(!failure.has_value() && std::get<0>(painted) == 1U && std::get<1>(painted) == inks &&
               layout_counts(layouts) == painted,
           "a large fence is laid out and coloured once, and repaints and scrolls redo nothing");
}

// lines 行の row の囲み（language）を 400 × 200 に描き、2 回目の描画と送りで layout の生成と色の
// 置き直しが 0 で、最初の描画で置いた色の数が inks であることを確かめる。長い囲みは切れの列で、
// 最初の描画が作って色を置くのは見えている最初の切れだけ（#1863）。
void check_fence_cost(std::size_t lines, std::string_view language, std::string_view row,
                      std::size_t inks)
{
    std::string text = "```" + std::string(language) + "\n";
    for (std::size_t line = 0; line < lines; ++line)
    {
        text += row;
        text += "\n";
    }
    const auto parsed = core::parse_markdown(text + "```\n", core::MarkdownStop::never());
    const auto colors = parsed ? parsed.value().blocks.front().code_colors : std::nullopt;
    if (!colors.has_value() || colors.value().size() != inks)
    {
        expect(false, "a large fence parses with the colours of its first piece");
        return;
    }
    check_fence_paint(parsed.value(), inks);
}

// 囲みのコードの構文の色（#1858）: 言語の分かった囲みは本文の字句器の役割の色を 1 回だけ置き、
// 描き直しは layout を作らず色も置き直さない。知らない言語の囲みは色を置かない（1 色）。
void check_code_colors()
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    const auto parsed = core::parse_markdown("```cpp\n// c\nint x = 1;\n```\n\n```nim\n// c\n```\n",
                                             core::MarkdownStop::never());
    if (!parsed || parsed.value().blocks.size() != 2U)
    {
        expect(false, "the code colour document parses into two fences");
        return;
    }
    win32::PreviewBrushes brushes;
    PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto tops = win32::PreviewBlockTops::measure(parsed.value(), layouts, style,
                                                       win32::PreviewViewport{0, 400.0F, 200.0F});
    if (target == nullptr || !theme || !tops ||
        FAILED(brushes.prepare(*target.Get(), theme.value().view())))
    {
        expect(false, "the code colour checks prepare a target, brushes and a document");
        return;
    }
    const win32::PreviewScene scene{
        parsed.value(), tops.value(), style, core::LayoutRect{0, 0, 400, 200}, 0.0F, std::nullopt};
    std::optional<win32::RenderFailure> failure;
    win32::PreviewPainter<ID2D1RenderTarget> painter(*target.Get(), layouts, brushes, failure);
    const auto first = painted_frame(*target.Get(), *bitmap.Get(), painter, scene);
    const auto painted = layout_counts(layouts);
    const auto second = painted_frame(*target.Get(), *bitmap.Get(), painter, scene);
    expect(!first.empty() && !failure.has_value() &&
               block_inked(layouts, scene, 0U, brushes.of(win32::PreviewInk::comment)),
           "a cpp fence draws its comment in the comment ink");
    expect(!block_inked(layouts, scene, 1U, brushes.of(win32::PreviewInk::comment)),
           "an unknown language keeps one colour");
    expect(layout_counts(layouts) == painted && second == first,
           "a repaint of a coloured fence makes no layout and sets no colour");
}

// 密な JSON の 1 行の色の区間の数。
[[nodiscard]] std::size_t json_row_colors()
{
    constexpr std::string_view row = "[1,2,3,4,5,6,7,8,9]\n";
    const auto colors = core::preview_code_piece_colors(core::Language::json, row,
                                                        core::preview_code_piece_starts(row))
                            .front();
    return colors.value_or(std::vector<core::PreviewCodeColor>{}).size();
}

// 区間の数が上限いっぱいの切れ（1 行に数を上限の数だけ）の色は最初に描くときに区間の数だけ 1 回
// 置き、描き直しも画面の送りも色を置き直さない（色の区間は要素が持つ）。長い囲み（1 行に数 1 つ）と
// 密な JSON の囲みは切れの列で、最初の描画が置くのは最初の切れの区間だけ（#1860 のレビュー M1・
// #1863）。
void check_large_fence_cost()
{
    std::string crowded = "1";
    for (std::size_t at = 1; at < core::preview_code_color_count_limit; ++at)
    {
        crowded += " 1";
    }
    check_fence_cost(1U, "py", crowded, core::preview_code_color_count_limit);
    check_fence_cost(core::preview_code_color_count_limit, "py", "1",
                     core::preview_code_piece_lines);
    check_fence_cost(1000U, "json", "[1,2,3,4,5,6,7,8,9]",
                     core::preview_code_piece_lines * json_row_colors());
}

// 囲みのコードの切れ（#1863）: 続く切れの間の空きは 0 で、境の上下の面の端（角の位置）も panel の
// 色（面が 1 枚につながる）。角を丸めるのは最初の切れの上と最後の切れの下だけで、切れの列の高さは
// 同じ行の 1 つの囲みと同じ。
void check_code_piece_pixels()
{
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    core::MarkdownDocument pieces;
    pieces.blocks.push_back(block_of(core::PreviewBlockKind::code, "a\nb\nc\n"));
    pieces.blocks.push_back(block_of(core::PreviewBlockKind::code, "d\ne\n"));
    pieces.blocks.front().piece = core::PreviewCodePiece::first;
    pieces.blocks.back().piece = core::PreviewCodePiece::last;
    core::MarkdownDocument joined;
    joined.blocks.push_back(block_of(core::PreviewBlockKind::code, "a\nb\nc\nd\ne\n"));
    PreviewTextLayouts layouts;
    const win32::PreviewViewport viewport{0, 400.0F, 200.0F};
    const auto tops = win32::PreviewBlockTops::measure(pieces, layouts, style_of(), viewport);
    const auto one = win32::PreviewBlockTops::measure(joined, layouts, style_of(), viewport);
    const auto pixels =
        theme ? directly_painted(pieces, theme.value().view(), core::LayoutRect{0, 0, 400, 200}, 0)
              : std::vector<std::uint8_t>{};
    if (!tops || !one || pixels.empty() || tops.value().size() != 2U)
    {
        expect(false, "the code piece sample measures and paints");
        return;
    }
    const auto &at = tops.value();
    const auto panel = theme.value().view().ui.panel;
    const auto seam = static_cast<std::int32_t>(at.bottom(0));
    const auto left = static_cast<std::int32_t>(at.indent(0));
    const auto right = 400 - static_cast<std::int32_t>(win32::preview_side_margin(96)) - 1;
    expect(at.top(1) == at.bottom(0) &&
               at.bottom(1) - at.top(0) == one.value().bottom(0) - one.value().top(0),
           "pieces follow without a gap and are as tall as one fence of the same lines");
    expect(distance_of(pixels, left, seam - 1, panel) <= 6 &&
               distance_of(pixels, left, seam, panel) <= 6 &&
               distance_of(pixels, right, seam - 1, panel) <= 6 &&
               distance_of(pixels, right, seam, panel) <= 6,
           "the panel joins across the piece boundary without corners");
    expect(distance_of(pixels, right, static_cast<std::int32_t>(at.top(0)), panel) > 6 &&
               distance_of(pixels, right, static_cast<std::int32_t>(at.bottom(1)) - 1, panel) > 6,
           "the first piece rounds its top corner and the last piece its bottom corner");
}

// 錨 0 から 400 × 200 に style で描いた画素と、測った上端の列（どちらか失敗なら空）。
[[nodiscard]] std::pair<std::vector<std::uint8_t>, std::optional<win32::PreviewBlockTops>>
painted_in(const core::MarkdownDocument &document, const core::Theme &theme,
           const win32::PreviewTextStyle &style)
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    PreviewTextLayouts layouts;
    win32::PreviewBrushes brushes;
    auto tops = win32::PreviewBlockTops::measure(document, layouts, style,
                                                 win32::PreviewViewport{0, 400.0F, 200.0F});
    if (target == nullptr || !tops || FAILED(brushes.prepare(*target.Get(), theme)))
    {
        return {};
    }
    std::optional<win32::RenderFailure> failure;
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    win32::PreviewPainter<ID2D1RenderTarget> painter(*target.Get(), layouts, brushes, failure);
    painter.paint(win32::PreviewScene{document, tops.value(), style,
                                      core::LayoutRect{0, 0, 400, 200}, 0.0F, std::nullopt});
    if (FAILED(target->EndDraw()) || failure.has_value())
    {
        return {};
    }
    return {pixels_of(*bitmap.Get()), std::move(tops).value()};
}

// 継ぎ目の y（seam）の前後 3 行の、面の右端と中ほどの画素が面の色か（許容は差 2）。
[[nodiscard]] bool seam_joined(const std::vector<std::uint8_t> &pixels, float seam,
                               core::RgbColor panel, std::uint32_t dpi)
{
    const auto row = static_cast<std::int32_t>(std::floor(seam));
    const auto right = 400 - static_cast<std::int32_t>(win32::preview_side_margin(dpi)) - 1;
    bool joined = true;
    for (const std::int32_t y : {row - 1, row, row + 1})
    {
        joined = joined && distance_of(pixels, right, y, panel) <= 2 &&
                 distance_of(pixels, 300, y, panel) <= 2;
    }
    return joined;
}

// 段落 1 つと、続く 2 つの切れ（first・last）の文書。
[[nodiscard]] core::MarkdownDocument seam_document()
{
    core::MarkdownDocument document;
    document.blocks.push_back(block_of(core::PreviewBlockKind::paragraph, "a"));
    document.blocks.push_back(block_of(core::PreviewBlockKind::code, "a\nb\n"));
    document.blocks.push_back(block_of(core::PreviewBlockKind::code, "c\nd\n"));
    document.blocks.at(1).piece = core::PreviewCodePiece::first;
    document.blocks.at(2).piece = core::PreviewCodePiece::last;
    return document;
}

// 継ぎ目を測る DPI（125% と 150%・#1913）。
constexpr std::array<std::uint32_t, 2> seam_dpis{120, 144};

// dpi で切れの継ぎ目が端数の y に来ても、継ぎ目の前後の行は面の色のまま（地の色が混ざった線が
// 出ない）。字の大きさは継ぎ目が画素の間に来る最初のものを選ぶ。
void check_code_piece_seam_at(const core::MarkdownDocument &document, const core::Theme &theme,
                              std::uint32_t dpi)
{
    for (const float size : {13.5F, 14.0F, 15.0F, 12.0F, 11.0F, 16.0F})
    {
        const auto [pixels, tops] =
            painted_in(document, theme, win32::PreviewTextStyle{"Segoe UI", size, dpi});
        if (pixels.empty() || !tops.has_value() || tops.value().size() != 3U)
        {
            expect(false, "the seam sample measures and paints");
            return;
        }
        const auto seam = tops.value().bottom(1);
        if (seam != std::floor(seam))
        {
            expect(seam_joined(pixels, seam, theme.ui.panel, dpi),
                   "a seam between pixels keeps the panel colour on its rows");
            return;
        }
    }
    expect(false, "a font size puts the seam between pixels");
}

// 切れの継ぎ目（囲みの上に段落・#1877 のレビュー R1）を 120 と 144 の DPI で確かめる。
void check_code_piece_seam()
{
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    if (!theme)
    {
        expect(false, "the theme for the seam check is found");
        return;
    }
    const auto document = seam_document();
    for (const auto dpi : seam_dpis)
    {
        check_code_piece_seam_at(document, theme.value().view(), dpi);
    }
}
// 枠より広い表の横の送り（#1883・施主決定 D107 の案 A）の見本。3 列の長い升の表（400 物理画素の枠に
// 入らない）と、その後の段落。
constexpr std::string_view wide_table = "| aaaaaaaaaaaaaaaaaaaaaaaa | bbbbbbbbbbbbbbbbbbbbbbbbbb | "
                                        "cccccccccccccccccccccccc |\n|---|---|---|\n| 1 | 2 | 3 |\n"
                                        "\nafter\n";

// 寸法の写し（物理画素 ↔ DIP・端・溝の位置）。
void check_table_scroll_metrics()
{
    const std::vector<float> narrow{100.0F, 200.0F};
    const std::vector<float> broad{300.0F, 200.0F};
    const auto fits = win32::preview_table_band(narrow, 400.0F, 16.0F, 96);
    const auto over = win32::preview_table_band(broad, 400.0F, 16.0F, 96);
    expect(fits.view == 368.0F && fits.width == 356.0F && !win32::preview_table_overflows(fits, 96),
           "a table narrower than the frame fits and has no bar");
    expect(win32::preview_table_overflows(over, 96) &&
               win32::preview_table_span(over, 96).view == 368 &&
               win32::preview_table_span(over, 96).table == 556,
           "a wider table overflows and its span is in DIP");
    expect(win32::preview_table_shift(over, 1000, 96) == 188.0F &&
               win32::preview_table_shift(over, -5, 96) == 0.0F &&
               win32::preview_table_shift(fits, 50, 96) == 0.0F,
           "the shift is the offset held between 0 and the end, and 0 for a table that fits");
    const std::vector<float> large{450.0F, 300.0F};
    const auto scaled = win32::preview_table_band(large, 600.0F, 24.0F, 144);
    expect(win32::preview_table_span(scaled, 144).view == 368 &&
               win32::preview_table_span(scaled, 144).table == 556 &&
               win32::preview_table_shift(scaled, 188, 144) == 282.0F &&
               win32::preview_table_track_pixels(scaled, 368, 144) == 552.0F &&
               win32::preview_table_track_dips(scaled, 552.0F, 144) == 368,
           "at 144 DPI the span and the track map pixels and DIP both ways");
    // 画素で収まる表は DIP に丸めても広くしない（150% で枠 751 px・表 750.5 px と、字下げが小数の
    // 100%・#1884 のレビュー M1）。画素で 0.5 だけ広い表は DIP で枠より 1 広い。
    const win32::PreviewTableBand rounded{751.0F, 750.5F};
    const auto fractional =
        win32::preview_table_band(std::vector<float>{339.3F}, 400.0F, 16.4F, 96);
    const win32::PreviewTableBand barely{750.5F, 751.0F};
    expect(!win32::preview_table_overflows(rounded, 144) &&
               !core::preview_table_wide(win32::preview_table_span(rounded, 144)) &&
               win32::preview_table_shift(rounded, 50, 144) == 0.0F,
           "a table that fits in pixels at 150% does not overflow after rounding to DIP");
    expect(fractional.width < fractional.view && !win32::preview_table_overflows(fractional, 96) &&
               !core::preview_table_wide(win32::preview_table_span(fractional, 96)),
           "a table that fits beside a fractional indent does not overflow");
    expect(win32::preview_table_overflows(barely, 144) &&
               core::preview_table_wide(win32::preview_table_span(barely, 144)),
           "a table wider by half a pixel overflows and its DIP span is wider too");
    expect(win32::preview_table_offset(std::vector<std::int32_t>{5, 7}, 1) == 7 &&
               win32::preview_table_offset(std::vector<std::int32_t>{5}, 3) == 0,
           "a table without an offset is unscrolled");
    const auto parsed =
        core::parse_markdown("| a |\n|---|\n| 1 |\n| 2 |\n\nx\n", core::MarkdownStop::never());
    expect(parsed.has_value() && !win32::preview_table_closes(parsed.value(), 0) &&
               !win32::preview_table_closes(parsed.value(), 1) &&
               win32::preview_table_closes(parsed.value(), 2) &&
               !win32::preview_table_closes(parsed.value(), 3),
           "only the last row of a table closes it");
}

// 広い表の最後の行だけが棒の分（4 + 6 DIP）高く、収まる表と途中の行は変わらない。
void check_table_scroll_heights()
{
    const auto wide = core::parse_markdown(wide_table, core::MarkdownStop::never());
    const auto narrow =
        core::parse_markdown("| a | b |\n|---|---|\n| 1 | 2 |\n", core::MarkdownStop::never());
    PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto line = win32::preview_line_pixels(style);
    const win32::PreviewViewport frame{0, 400.0F, 200.0F};
    const auto wide_tops =
        wide ? win32::PreviewBlockTops::measure(wide.value(), layouts, style, frame)
             : std::unexpected(win32::RenderFailure::directwrite);
    const auto narrow_tops =
        narrow ? win32::PreviewBlockTops::measure(narrow.value(), layouts, style, frame)
               : std::unexpected(win32::RenderFailure::directwrite);
    if (!wide_tops || !narrow_tops || wide_tops.value().size() < 3)
    {
        expect(false, "the table samples are measured");
        return;
    }
    const auto &at = wide_tops.value();
    expect(at.bottom(0) - at.top(0) == line + 14.0F && at.bottom(1) - at.top(1) == line + 24.0F,
           "only the last row of a wide table grows by the bar and its gap");
    expect(at.top(2) > at.bottom(1), "the next block follows the bar");
    const auto &fits = narrow_tops.value();
    expect(fits.bottom(1) - fits.top(1) == line + 14.0F, "a table that fits does not grow");
}

// 描く: 溝は panel_border・つまみは muted（明るい表は text）で送りの量の位置。送った升は枠の左で
// 切る（字下げの左に字を描かない）。
void check_table_scroll_pixels()
{
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    const auto parsed = core::parse_markdown(wide_table, core::MarkdownStop::never());
    PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto tops =
        parsed ? win32::PreviewBlockTops::measure(parsed.value(), layouts, style,
                                                  win32::PreviewViewport{0, 400.0F, 200.0F})
               : std::unexpected(win32::RenderFailure::directwrite);
    if (!theme || !parsed || !tops)
    {
        expect(false, "the wide table sample parses and is measured");
        return;
    }
    const auto &palette = theme.value().view();
    const core::LayoutRect area{0, 0, 400, 200};
    const auto start = directly_painted(parsed.value(), palette, area, 0);
    const auto end = painted_with(parsed.value(), palette, PaintPlace{area, 0},
                                  TableScrolled{{10'000}, std::nullopt});
    const auto lit =
        painted_with(parsed.value(), palette, PaintPlace{area, 0}, TableScrolled{{}, 0U});
    if (start.empty() || end.empty() || lit.empty())
    {
        expect(false, "the wide table paints");
        return;
    }
    const auto &at = tops.value();
    const auto left = static_cast<std::int32_t>(at.indent(1));
    const auto right = 400 - 16;
    const auto bar = static_cast<std::int32_t>(at.bottom(1)) - 3;
    expect(distance_of(start, left + 4, bar, palette.ui.muted) <= 6 &&
               distance_of(start, right - 4, bar, palette.ui.panel_border) <= 6,
           "the unscrolled thumb is muted at the left of the panel_border track");
    expect(distance_of(end, left + 4, bar, palette.ui.panel_border) <= 6 &&
               distance_of(end, right - 4, bar, palette.ui.muted) <= 6,
           "the scrolled thumb sits at the right end");
    expect(distance_of(lit, left + 4, bar, palette.ui.text) <= 6,
           "a lit thumb is drawn in the text colour");
    expect(distance_of(end, left - 2, static_cast<std::int32_t>(at.top(0)) + 12,
                       palette.body.background) <= 6 &&
               start != end,
           "a scrolled table moves its cells and is clipped at the left of the frame");
    expect(distance_of(start, right + 3, static_cast<std::int32_t>(at.top(0)) + 12,
                       palette.body.background) <= 6 &&
               distance_of(start, right + 3, static_cast<std::int32_t>(at.top(1)) + 12,
                           palette.body.background) <= 6,
           "the cells that run past the frame are clipped at its right margin");
}

// 当たり: 棒と行は表、字下げの左・後の段落は表の外。つかんだ表の溝の位置。送った表の升の当たりは
// 描く所と同じ送りを読む。
void check_table_scroll_hits()
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    auto parsed = core::parse_markdown(wide_table, core::MarkdownStop::never());
    win32::PreviewPane pane;
    if (target == nullptr || !theme || !parsed ||
        !pane.prepare(*target.Get(), theme.value().view()).has_value())
    {
        expect(false, "the preview pane prepares for the wide table");
        return;
    }
    const std::shared_ptr<const core::MarkdownDocument> shown =
        std::make_shared<const core::MarkdownDocument>(std::move(parsed).value());
    const auto style = style_of();
    const core::LayoutRect area{0, 0, 400, 200};
    PreviewTextLayouts layouts;
    const auto tops = win32::PreviewBlockTops::measure(*shown, layouts, style,
                                                       win32::PreviewViewport{0, 400.0F, 200.0F});
    const auto columns = layouts.table_columns(*shown, 0, style);
    target->BeginDraw();
    const auto painted = pane.paint(
        *target.Get(),
        application::PreviewFrame{shown, core::Offset{0}, {}, {}, nullptr, {50}, std::nullopt},
        style, area);
    if (FAILED(target->EndDraw()) || !painted || !tops || !columns)
    {
        expect(false, "the wide table is painted by the pane");
        return;
    }
    const auto &at = tops.value();
    const auto left = static_cast<std::int32_t>(at.indent(1));
    const auto on_bar = pane.table_at(left + 20, static_cast<std::int32_t>(at.bottom(1)) - 3);
    expect(on_bar.has_value() && on_bar.value().bar && on_bar.value().table == 0 &&
               on_bar.value().span.view == 368 && on_bar.value().at == 20,
           "a point on the bar hits the table's bar at its track position");
    const auto on_row = pane.table_at(left + 20, static_cast<std::int32_t>(at.top(0)) + 5);
    expect(on_row.has_value() && !on_row.value().bar, "a point on a row hits the table");
    expect(!pane.table_at(left - 4, static_cast<std::int32_t>(at.top(0)) + 5).has_value() &&
               !pane.table_at(left + 20, static_cast<std::int32_t>(at.top(2)) + 3).has_value(),
           "the indent and the next paragraph are outside the table");
    const auto dragged = pane.table_bar(0, left + 100);
    expect(dragged.has_value() && dragged.value().at == 100,
           "a grabbed table reads the pointer along its track");
    const auto shift = win32::preview_table_shift(
        win32::preview_table_band(*columns.value(), 400.0F, at.indent(0), 96), 50, 96);
    const auto cell =
        static_cast<std::int32_t>(at.indent(0) +
                                  win32::preview_table_cell_left(*columns.value(), 1, shift, 96)) +
        3;
    const auto point = pane.point_at(cell, static_cast<std::int32_t>(at.top(0)) + 12);
    expect(shift == 50.0F && point.has_value() && point.value().block == 0 &&
               point.value().offset >= core::preview_table_cell_range(shown->blocks.at(0), 1).first,
           "the cell under a point follows the same shift as the painting");
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
    check_repaint_keeps_layouts_clean();
    check_target_change_drops_inked();
    check_recolour_every_ink();
    check_code_colors();
    check_large_fence_cost();
    check_code_piece_pixels();
    check_code_piece_seam();
    check_unit_ranges_match_units();
    check_html_and_style();
    check_markers();
    check_pane_reuse();
    check_pane_picture();
    check_full_preview_band();
    check_measured_from_anchor();
    check_list_scanned_in_range();
    check_anchor_after_carriage_returns();
    check_task_boxes();
    check_task_pixels();
    check_task_press();
    check_selection_pixels();
    check_strikethrough_pixels();
    check_front_matter_pixels();
    check_table_pixels();
    check_alert_pixels();
    check_large_table_cost();
    check_many_tables_kept();
    check_table_scroll_metrics();
    check_table_scroll_heights();
    check_table_scroll_pixels();
    check_table_scroll_hits();
}
} // namespace nenenib::tests::ui
