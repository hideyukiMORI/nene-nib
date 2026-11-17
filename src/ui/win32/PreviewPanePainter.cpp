#include "PreviewPanePainter.hpp"

#include "DevicePixels.hpp"
#include "PaletteRowKey.hpp"
#include "PaletteTextRole.hpp"
#include "PreviewColor.hpp"
#include "RgbaColor.hpp"
#include "SyntaxColor.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
constexpr float full_channel = 255.0F;
constexpr float base_dpi = 96.0F;
// 見出しの文の間（採用案の gap 10 DIP）。
constexpr std::int32_t heading_gap_dips = 10;
// 切り替えの印（見出しの右端の 28 DIP 四方の中の 14 × 10 DIP の枠）。
constexpr float toggle_width_dips = 14.0F;
constexpr float toggle_height_dips = 10.0F;
// 1 行の当たりが分かれる矩形の数の上限（双方向の字の並びで分かれる）。
constexpr std::size_t match_runs = 8;

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbaColor color) noexcept
{
    D2D1_COLOR_F translucent = to_color(color.color);
    translucent.a = static_cast<float>(color.alpha) / full_channel;
    return translucent;
}

[[nodiscard]] D2D1_RECT_F to_rect(const core::LayoutRect &area) noexcept
{
    return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                       static_cast<float>(area.right), static_cast<float>(area.bottom));
}

[[nodiscard]] UINT32 utf16_at(std::string_view text, std::size_t byte)
{
    return static_cast<UINT32>(
        core::utf16_length(text.substr(0, std::min(byte, text.size()))).value_or(0));
}
// 区間の全部の矩形（数を問うてから受ける・固定の上限で色を落とさない）。区間が空・失敗は空。
[[nodiscard]] std::vector<DWRITE_HIT_TEST_METRICS>
hit_runs(IDWriteTextLayout &layout, DWRITE_TEXT_RANGE range, const core::LayoutRect &area)
{
    UINT32 count = 0;
    const auto origin_x = static_cast<float>(area.left);
    const auto origin_y = static_cast<float>(area.top);
    if (range.length == 0)
    {
        return {};
    }
    const auto asked = layout.HitTestTextRange(range.startPosition, range.length, origin_x,
                                               origin_y, nullptr, 0, &count);
    if (FAILED(asked) && asked != E_NOT_SUFFICIENT_BUFFER)
    {
        return {};
    }
    std::vector<DWRITE_HIT_TEST_METRICS> runs(count);
    if (FAILED(layout.HitTestTextRange(range.startPosition, range.length, origin_x, origin_y,
                                       runs.data(), count, &count)))
    {
        return {};
    }
    runs.resize(count);
    return runs;
}
} // namespace

PreviewPanePainter::PreviewPanePainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                                       const PreviewPaneText &text,
                                       PaletteRowLayouts &layouts) noexcept
    : target_(target), brush_(brush), text_(text), layouts_(layouts)
{
}

void PreviewPanePainter::paint(const std::optional<application::PreviewSnippet> &snippet,
                               const core::PaletteLayout &layout, const core::Palette &palette,
                               const core::SyntaxPalette &syntax)
{
    syntax_ = &syntax;
    if (layout.preview == core::FinderPreview::off || core::width_of(layout.pane) <= 0 ||
        core::height_of(layout.pane) <= 0)
    {
        return;
    }
    paint_ground(layout, palette);
    paint_toggle(layout, palette);
    if (!snippet.has_value())
    {
        return;
    }
    target_.PushAxisAlignedClip(to_rect(layout.pane), D2D1_ANTIALIAS_MODE_ALIASED);
    paint_heading(snippet.value(), layout, palette);
    paint_lines(snippet.value(), layout, palette);
    target_.PopAxisAlignedClip();
}

// 地と、当たりの欄との境・見出しの下の 1 DIP の線（採用案の「欄の境」）。
void PreviewPanePainter::paint_ground(const core::PaletteLayout &layout,
                                      const core::Palette &palette)
{
    const auto &pane = layout.pane;
    brush_.SetColor(to_color(palette.background));
    target_.FillRectangle(to_rect(pane), &brush_);
    const float half = 0.5F;
    const auto left = static_cast<float>(pane.left) + half;
    const auto top = static_cast<float>(pane.top) + half;
    if (layout.preview == core::FinderPreview::right)
    {
        line_between(D2D1::Point2F(left, static_cast<float>(pane.top)),
                     D2D1::Point2F(left, static_cast<float>(pane.bottom)), palette.panel_border);
    }
    else
    {
        line_between(D2D1::Point2F(static_cast<float>(pane.left), top),
                     D2D1::Point2F(static_cast<float>(pane.right), top), palette.panel_border);
    }
    const auto under = static_cast<float>(layout.pane_header.bottom) - half;
    line_between(D2D1::Point2F(static_cast<float>(pane.left), under),
                 D2D1::Point2F(static_cast<float>(pane.right), under), palette.panel_border);
}

// 描いている形の印（枠と、right は縦・below は横の仕切り）。muted の 1 DIP の線。
void PreviewPanePainter::paint_toggle(const core::PaletteLayout &layout,
                                      const core::Palette &palette)
{
    const auto &toggle = layout.pane_toggle;
    if (core::width_of(toggle) <= 0 || core::height_of(toggle) <= 0)
    {
        return;
    }
    const float scale = static_cast<float>(text_.dpi) / base_dpi;
    const float width = toggle_width_dips * scale;
    const float height = toggle_height_dips * scale;
    const float left =
        std::round((static_cast<float>(toggle.left + toggle.right) - width) / 2) + 0.5F;
    const float top =
        std::round((static_cast<float>(toggle.top + toggle.bottom) - height) / 2) + 0.5F;
    brush_.SetColor(to_color(palette.muted));
    target_.DrawRectangle(D2D1::RectF(left, top, left + width, top + height), &brush_, scale);
    if (layout.preview == core::FinderPreview::right)
    {
        const float split = std::round(left + width * 0.4F) + 0.5F;
        line_between(D2D1::Point2F(split, top), D2D1::Point2F(split, top + height), palette.muted);
    }
    else
    {
        const float split = std::round(top + height * 0.5F) + 0.5F;
        line_between(D2D1::Point2F(left, split), D2D1::Point2F(left + width, split), palette.muted);
    }
}

void PreviewPanePainter::paint_heading(const application::PreviewSnippet &snippet,
                                       const core::PaletteLayout &layout,
                                       const core::Palette &palette)
{
    const auto heading = core::palette_preview_heading(layout, text_.dpi);
    if (core::width_of(heading) <= 0 || core::height_of(heading) <= 0)
    {
        return;
    }
    const auto limit = paint_heading_right(snippet, heading, palette);
    const core::LayoutRect named{heading.left, heading.top, std::max(heading.left, limit),
                                 heading.bottom};
    const auto name = obtained(snippet.name.text(), text_.name, named);
    if (!name)
    {
        return;
    }
    draw(*name.Get(), named.left, named.top, palette.text);
    if (!snippet.place.has_value())
    {
        return;
    }
    const auto gap = core::to_pixels(heading_gap_dips, text_.dpi);
    const auto left = std::min(named.left + measured_width(*name.Get()) + gap, named.right);
    const core::LayoutRect placed{left, heading.top, named.right, heading.bottom};
    const auto place = obtained(snippet.place.value().text(), text_.note, placed);
    if (place)
    {
        target_.PushAxisAlignedClip(to_rect(placed), D2D1_ANTIALIAS_MODE_ALIASED);
        draw(*place.Get(), placed.left, placed.top, palette.muted);
        target_.PopAxisAlignedClip();
    }
}

std::int32_t PreviewPanePainter::paint_heading_right(const application::PreviewSnippet &snippet,
                                                     const core::LayoutRect &heading,
                                                     const core::Palette &palette)
{
    const auto gap = core::to_pixels(heading_gap_dips, text_.dpi);
    auto right = heading.right;
    const std::array<std::string_view, 2> items{snippet.language, snippet.line.has_value()
                                                                      ? snippet.line.value().text()
                                                                      : std::string_view{}};
    for (const std::string_view item : items)
    {
        if (item.empty())
        {
            continue;
        }
        const auto shown = obtained(item, text_.note, heading);
        if (!shown)
        {
            continue;
        }
        const auto width = measured_width(*shown.Get());
        if (right - width < heading.left)
        {
            break;
        }
        draw(*shown.Get(), right - width, heading.top, palette.muted);
        right -= width + gap;
    }
    return right;
}

void PreviewPanePainter::paint_lines(const application::PreviewSnippet &snippet,
                                     const core::PaletteLayout &layout,
                                     const core::Palette &palette)
{
    const auto fits = core::palette_preview_lines(layout, text_.dpi);
    if (fits == 0)
    {
        return;
    }
    if (snippet.notice.has_value())
    {
        const auto area =
            core::palette_preview_text(core::palette_preview_line(layout, text_.dpi, 0), text_.dpi);
        const auto notice = obtained(snippet.notice.value().text(), text_.note, area);
        if (notice)
        {
            draw(*notice.Get(), area.left, area.top, palette.muted);
        }
        return;
    }
    const auto count = std::min(fits, snippet.lines.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        paint_line(snippet.lines.at(index), core::palette_preview_line(layout, text_.dpi, index),
                   palette);
    }
}

void PreviewPanePainter::paint_line(const application::PreviewLine &line,
                                    const core::LayoutRect &area, const core::Palette &palette)
{
    if (line.hit)
    {
        brush_.SetColor(to_color(palette.current_line));
        target_.FillRectangle(to_rect(area), &brush_);
    }
    const auto number_area = core::palette_preview_number(area, text_.dpi);
    const auto number = obtained(std::to_string(line.number.value), text_.number, number_area);
    if (number)
    {
        draw(*number.Get(), number_area.left, number_area.top,
             line.hit ? palette.text : palette.gutter);
    }
    const auto text_area = core::palette_preview_text(area, text_.dpi);
    const auto shown = obtained(line.text, text_.line, text_area);
    if (!shown)
    {
        return;
    }
    target_.PushAxisAlignedClip(to_rect(text_area), D2D1_ANTIALIAS_MODE_ALIASED);
    mark_match(*shown.Get(), line, text_area, palette);
    draw(*shown.Get(), text_area.left, text_area.top, palette.text);
    tint_colors(*shown.Get(), line, text_area);
    target_.PopAxisAlignedClip();
}

void PreviewPanePainter::tint_colors(IDWriteTextLayout &layout,
                                     const application::PreviewLine &line,
                                     const core::LayoutRect &area)
{
    if (syntax_ == nullptr)
    {
        return;
    }
    for (const application::PreviewColor &color : line.colors)
    {
        const auto begin = utf16_at(line.text, color.begin);
        const auto end = utf16_at(line.text, color.end);
        for (const auto &run :
             hit_runs(layout, DWRITE_TEXT_RANGE{begin, end > begin ? end - begin : 0U}, area))
        {
            target_.PushAxisAlignedClip(
                D2D1::RectF(run.left, run.top, run.left + run.width, run.top + run.height),
                D2D1_ANTIALIAS_MODE_ALIASED);
            draw(layout, area.left, area.top, core::syntax_color(*syntax_, color.role));
            target_.PopAxisAlignedClip();
        }
    }
}

// 当たりの文字の後ろに search の面と accent の 1 DIP の枠（D17 と同じ形）。
void PreviewPanePainter::mark_match(IDWriteTextLayout &layout, const application::PreviewLine &line,
                                    const core::LayoutRect &area, const core::Palette &palette)
{
    if (!line.hit || line.match_end <= line.match_begin)
    {
        return;
    }
    const auto begin = utf16_at(line.text, line.match_begin);
    const auto end = utf16_at(line.text, line.match_end);
    std::array<DWRITE_HIT_TEST_METRICS, match_runs> runs{};
    UINT32 count = 0;
    if (end <= begin ||
        FAILED(layout.HitTestTextRange(begin, end - begin, static_cast<float>(area.left),
                                       static_cast<float>(area.top), runs.data(),
                                       static_cast<UINT32>(runs.size()), &count)))
    {
        return;
    }
    const float scale = static_cast<float>(text_.dpi) / base_dpi;
    for (std::size_t index = 0; index < std::min<std::size_t>(count, runs.size()); ++index)
    {
        const auto &run = runs.at(index);
        const auto face =
            D2D1::RectF(run.left, run.top, run.left + run.width, run.top + run.height);
        brush_.SetColor(to_color(palette.search));
        target_.FillRectangle(face, &brush_);
        brush_.SetColor(to_color(palette.accent));
        const float half = scale / 2;
        target_.DrawRectangle(
            D2D1::RectF(face.left + half, face.top + half, face.right - half, face.bottom - half),
            &brush_, scale);
    }
}

std::int32_t PreviewPanePainter::measured_width(IDWriteTextLayout &layout)
{
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(layout.GetMetrics(&metrics)))
    {
        return 0;
    }
    return static_cast<std::int32_t>(std::ceil(metrics.widthIncludingTrailingWhitespace));
}

void PreviewPanePainter::line_between(D2D1_POINT_2F from, D2D1_POINT_2F to, core::RgbColor color)
{
    brush_.SetColor(to_color(color));
    target_.DrawLine(from, to, &brush_, static_cast<float>(text_.dpi) / base_dpi);
}

void PreviewPanePainter::draw(IDWriteTextLayout &layout, std::int32_t x, std::int32_t y,
                              core::RgbColor color)
{
    brush_.SetColor(to_color(color));
    target_.DrawTextLayout(D2D1::Point2F(static_cast<float>(x), static_cast<float>(y)), &layout,
                           &brush_, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

PaletteRowLayouts::TextLayout PreviewPanePainter::obtained(std::string_view text,
                                                           IDWriteTextFormat *format,
                                                           const core::LayoutRect &area)
{
    return layouts_.obtain(PaletteRowKey{format, PaletteTextRole::plain, text, area, text_.dpi},
                           [&] { return made(text, format, area); });
}

PaletteRowLayouts::TextLayout PreviewPanePainter::made(std::string_view text,
                                                       IDWriteTextFormat *format,
                                                       const core::LayoutRect &area) const
{
    const std::wstring wide = core::to_utf16(text).value_or(std::wstring{});
    PaletteRowLayouts::TextLayout layout;
    if (text_.dwrite == nullptr || format == nullptr ||
        FAILED(text_.dwrite->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()),
                                              format, static_cast<float>(core::width_of(area)),
                                              static_cast<float>(core::height_of(area)),
                                              layout.GetAddressOf())))
    {
        return nullptr;
    }
    return layout;
}
} // namespace nenenib::ui::win32
