// #1785 段 P1（ADR 0189 の決定 5・採用案 docs/design/2026-11-16-finder-look.md の「置き方」
// 「プレビュー欄」）: プレビュー欄の画素。WIC の bitmap（SoftwareTarget）に PreviewPanePainter で
// 描き、right（左の縦の境）・below（上の横の境）・off（何も描かない）を、ダークとライトで確かめる。
// 欄の地は background、境と見出しの下の線は panel_border、当たりの行は current_line の面、当たりの
// 文字は search の面 ＋ accent の 1 DIP の枠、見出しに墨、切り替えの印に muted の線、中身を出せない
// ときは 1 行の文だけ。文字組みの保持は上限の中で、描き直しで増えない。配置は製品と同じ形の
// core::PaletteLayout を bitmap の高さ（80 画素）に合わせて組む。窓・device は作らない。字の画素は
// 字体に依るので、Segoe UI と Consolas が無い機械では測らない（QLT-013）。
#include "Appearance.hpp"
#include "DirectWriteFactory.hpp"
#include "DisplayText.hpp"
#include "FinderPreview.hpp"
#include "LayoutRect.hpp"
#include "LineNumber.hpp"
#include "Palette.hpp"
#include "PaletteLayout.hpp"
#include "PaletteRowLayouts.hpp"
#include "PreviewColor.hpp"
#include "PreviewLine.hpp"
#include "PreviewPanePainter.hpp"
#include "PreviewPaneText.hpp"
#include "PreviewSnippet.hpp"
#include "SoftwareTarget.hpp"
#include "SyntaxPalette.hpp"
#include "SyntaxRole.hpp"
#include "WindowChecks.hpp"

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
namespace app = nenenib::application;
namespace core = nenenib::core;
using nenenib::ui::win32::PaletteRowLayouts;
using nenenib::ui::win32::PreviewPanePainter;
using nenenib::ui::win32::PreviewPaneText;
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using Pixels = SoftwareTarget::Pixels;

constexpr std::uint32_t dpi = 96;
constexpr std::size_t channels = 4;
constexpr std::int32_t heading = 28;

[[nodiscard]] Format format_of(IDWriteFactory2 &factory, const wchar_t *family, float size,
                               DWRITE_FONT_WEIGHT weight)
{
    Format format;
    if (FAILED(factory.CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                        DWRITE_FONT_STRETCH_NORMAL, size, L"",
                                        format.GetAddressOf())))
    {
        return nullptr;
    }
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return format;
}

// 製品の見出しの書式と同じ文字単位の省略記号を足す（作れなければ null）。
[[nodiscard]] Format trimmed(IDWriteFactory2 &factory, Format format)
{
    Microsoft::WRL::ComPtr<IDWriteInlineObject> sign;
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    if (format == nullptr ||
        FAILED(factory.CreateEllipsisTrimmingSign(format.Get(), sign.GetAddressOf())) ||
        FAILED(format->SetTrimming(&trimming, sign.Get())))
    {
        return nullptr;
    }
    return format;
}

[[nodiscard]] bool pixel_is(const Pixels &pixels, std::int32_t x, std::int32_t y,
                            core::RgbColor color)
{
    const auto at =
        ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) *
        channels;
    return pixels.size() > at + 3 && pixels.at(at) == color.blue &&
           pixels.at(at + 1) == color.green && pixels.at(at + 2) == color.red;
}

// 矩形の中で color の画素の数。
[[nodiscard]] std::size_t count_in(const Pixels &pixels, const core::LayoutRect &area,
                                   core::RgbColor color)
{
    std::size_t count = 0;
    for (auto y = area.top; y < area.bottom; ++y)
    {
        for (auto x = area.left; x < area.right; ++x)
        {
            count += pixel_is(pixels, x, y, color) ? 1 : 0;
        }
    }
    return count;
}

[[nodiscard]] std::size_t ink_in(const Pixels &pixels, const core::LayoutRect &area,
                                 core::RgbColor ground)
{
    return static_cast<std::size_t>(core::width_of(area)) *
               static_cast<std::size_t>(core::height_of(area)) -
           count_in(pixels, area, ground);
}

// 2 枚の絵で違う画素の数。
[[nodiscard]] std::size_t differing(const Pixels &left, const Pixels &right,
                                    const core::LayoutRect &area)
{
    std::size_t count = 0;
    for (auto y = area.top; y < area.bottom; ++y)
    {
        for (auto x = area.left; x < area.right; ++x)
        {
            const auto at = ((static_cast<std::size_t>(y) * SoftwareTarget::width) +
                             static_cast<std::size_t>(x)) *
                            channels;
            count += left.at(at) != right.at(at) || left.at(at + 1) != right.at(at + 1) ||
                             left.at(at + 2) != right.at(at + 2)
                         ? 1
                         : 0;
        }
    }
    return count;
}

// bitmap の高さに入る欄（見出し 28 画素・本文 52 画素で 2 行）を、製品と同じ切り出しで組む。
[[nodiscard]] core::PaletteLayout layout_of(core::FinderPreview preview,
                                            const core::LayoutRect &pane)
{
    core::PaletteLayout layout{pane, pane, pane, pane, 40, 1};
    layout.preview = preview;
    layout.pane = pane;
    layout.pane_header = core::LayoutRect{pane.left, pane.top, pane.right, pane.top + heading};
    layout.pane_body = core::LayoutRect{pane.left, pane.top + heading, pane.right, pane.bottom};
    layout.pane_toggle =
        core::LayoutRect{pane.right - heading, pane.top, pane.right, pane.top + heading};
    return layout;
}

[[nodiscard]] app::PreviewSnippet snippet_of(std::size_t match_end)
{
    std::vector<app::PreviewLine> lines;
    lines.push_back(app::PreviewLine{core::LineNumber{411}, "    // before the hit"});
    lines.push_back(app::PreviewLine{core::LineNumber{412}, "    auto step = measured(key);", true,
                                     16, match_end});
    return app::PreviewSnippet{core::DisplayText::parse("EditorController.cpp").value(),
                               core::DisplayText::parse("src/application").value(),
                               core::DisplayText::parse("412").value(),
                               "cpp",
                               std::move(lines),
                               std::nullopt};
}

// 構文の色の試験の色（keyword だけ純粋な緑・ほかは本文の色）。
[[nodiscard]] core::SyntaxPalette syntax_of(const core::Palette &palette)
{
    const auto text = palette.text;
    auto syntax = core::SyntaxPalette{text, text, text, text, text, text, text, text,
                                      text, text, text, text, text, text, text, text};
    syntax.keyword = core::RgbColor{0, 255, 0};
    return syntax;
}

class PaneStrip final
{
  public:
    PaneStrip(SoftwareTarget &target, const PreviewPaneText &text, PaletteRowLayouts &layouts,
              const core::Palette &palette)
        : target_(target), text_(text), layouts_(layouts), palette_(palette),
          syntax_(syntax_of(palette))
    {
    }
    [[nodiscard]] Pixels operator()(const std::optional<app::PreviewSnippet> &snippet,
                                    const core::PaletteLayout &layout) const
    {
        return target_.painted(
            [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
            {
                const auto &panel = palette_.panel;
                brush->SetColor(D2D1::ColorF(static_cast<float>(panel.red) / 255.0F,
                                             static_cast<float>(panel.green) / 255.0F,
                                             static_cast<float>(panel.blue) / 255.0F, 1.0F));
                surface.FillRectangle(
                    D2D1::RectF(0.0F, 0.0F, SoftwareTarget::width, SoftwareTarget::height), brush);
                PreviewPanePainter painter(surface, *brush, text_, layouts_);
                painter.paint(snippet, layout, palette_, syntax_);
            });
    }

  private:
    SoftwareTarget &target_;
    const PreviewPaneText &text_;
    PaletteRowLayouts &layouts_;
    const core::Palette &palette_;
    core::SyntaxPalette syntax_;
};

void verify_right(const PaneStrip &strip, PaletteRowLayouts &layouts, const core::Palette &palette)
{
    const core::LayoutRect pane{700, 0, 1300, 80};
    const auto layout = layout_of(core::FinderPreview::right, pane);
    const auto marked = strip(snippet_of(24), layout);
    const auto made = layouts.size();
    static_cast<void>(strip(snippet_of(24), layout));
    expect(made > 0 && made <= PreviewPanePainter::layout_limit && layouts.size() == made,
           "the pane keeps its text layouts within the limit and painting again adds none");
    expect(pixel_is(marked, pane.left + 300, 76, palette.background),
           "the pane ground is the body background");
    expect(pixel_is(marked, pane.left, 76, palette.panel_border),
           "right: a 1 DIP panel_border line runs down the left edge of the pane");
    expect(pixel_is(marked, pane.left + 3, layout.pane_header.bottom - 1, palette.panel_border),
           "a 1 DIP panel_border line runs under the heading");
    const auto hit_line = core::palette_preview_line(layout, dpi, 1);
    expect(pixel_is(marked, hit_line.right - 3, (hit_line.top + hit_line.bottom) / 2,
                    palette.current_line),
           "the hit line gets the current_line face");
    const auto plain = strip(snippet_of(16), layout);
    const auto text_area = core::palette_preview_text(hit_line, dpi);
    expect(differing(marked, plain, text_area) > 0 &&
               count_in(marked, text_area, palette.accent) > 0,
           "the hit's characters get the search face with an accent outline");
    expect(differing(marked, plain, core::palette_preview_line(layout, dpi, 0)) == 0,
           "the line before the hit is not marked");
    // 構文の色（段 P2）: 区間の文字だけが keyword の色で描き直され、区間の外は変わらない。
    auto colored = snippet_of(16);
    colored.lines.at(1).colors.push_back(app::PreviewColor{4, 8, core::SyntaxRole::keyword});
    const auto tinted = strip(colored, layout);
    const auto keyword_area =
        core::LayoutRect{text_area.left, text_area.top, text_area.left + 80, text_area.bottom};
    expect(differing(tinted, plain, keyword_area) > 0 &&
               differing(tinted, plain, core::palette_preview_line(layout, dpi, 0)) == 0,
           "a colored span is drawn in its syntax color and the other lines stay as they were");
    const auto heading_area = core::palette_preview_heading(layout, dpi);
    expect(ink_in(marked, heading_area, palette.background) > 0 &&
               count_in(marked, layout.pane_toggle, palette.muted) > 0,
           "the heading carries the name and the toggle shows the drawn form");
    const auto notice =
        strip(app::PreviewSnippet{core::DisplayText::parse("far.txt").value(),
                                  std::nullopt,
                                  std::nullopt,
                                  "text",
                                  {},
                                  core::DisplayText::parse("open tabs only").value()},
              layout);
    expect(ink_in(notice, text_area, palette.background) == 0 &&
               ink_in(notice,
                      core::palette_preview_text(core::palette_preview_line(layout, dpi, 0), dpi),
                      palette.background) > 0,
           "a preview without content writes one line and nothing below it");
}

void verify_below_and_off(const PaneStrip &strip, const core::Palette &palette)
{
    const core::LayoutRect pane{0, 0, 640, 80};
    const auto below = strip(snippet_of(24), layout_of(core::FinderPreview::below, pane));
    expect(pixel_is(below, pane.left + 3, pane.top, palette.panel_border) &&
               pixel_is(below, pane.right - 60, 76, palette.background),
           "below: a 1 DIP panel_border line runs along the top of the pane");
    const auto off = strip(snippet_of(24), layout_of(core::FinderPreview::off, pane));
    expect(count_in(off, pane, palette.panel) ==
               static_cast<std::size_t>(core::width_of(pane)) *
                   static_cast<std::size_t>(core::height_of(pane)),
           "off: nothing is painted");
}
} // namespace

void verify_finder_preview_paint()
{
    SoftwareTarget target;
    const auto factory = directwrite_factory();
    if (!target.ready() || factory == nullptr)
    {
        unmeasured("the preview pane needs WIC and DirectWrite");
        return;
    }
    if (!installed(*factory.Get(), L"Segoe UI") || !installed(*factory.Get(), L"Consolas"))
    {
        unmeasured("the preview pane pixels need Segoe UI and Consolas");
        return;
    }
    const std::vector<Format> formats{
        format_of(*factory.Get(), L"Consolas", 12.5F, DWRITE_FONT_WEIGHT_NORMAL),
        format_of(*factory.Get(), L"Consolas", 12.5F, DWRITE_FONT_WEIGHT_NORMAL),
        trimmed(*factory.Get(),
                format_of(*factory.Get(), L"Segoe UI", 11.5F, DWRITE_FONT_WEIGHT_SEMI_BOLD)),
        trimmed(*factory.Get(),
                format_of(*factory.Get(), L"Segoe UI", 11.5F, DWRITE_FONT_WEIGHT_NORMAL))};
    for (const auto &format : formats)
    {
        if (format == nullptr)
        {
            unmeasured("the preview pane formats could not be made");
            return;
        }
    }
    formats.at(1)->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    const PreviewPaneText text{factory.Get(),       formats.at(0).Get(), formats.at(1).Get(),
                               formats.at(2).Get(), formats.at(3).Get(), dpi};
    for (const auto appearance : {core::Appearance::dark, core::Appearance::light})
    {
        const auto palette = core::palette_for(appearance);
        PaletteRowLayouts layouts(PreviewPanePainter::layout_limit);
        const PaneStrip strip(target, text, layouts, palette);
        verify_right(strip, layouts, palette);
        verify_below_and_off(strip, palette);
    }
}
} // namespace nenenib::tests::ui
