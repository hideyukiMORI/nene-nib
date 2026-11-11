#include "ExplorerPainter.hpp"

#include "DevicePixels.hpp"
#include "FilePath.hpp"
#include "PaletteRowKey.hpp"
#include "PaletteTextRole.hpp"
#include "SyntaxColor.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string>

namespace nenenib::ui::win32
{
namespace
{
constexpr float full_channel = 255.0F;
// 「…」の行（2,000 件を超えたフォルダ）。
constexpr std::string_view more_mark = "…";
// ルートが無い面の先頭の行（採用案の表・文は今のまま）。
constexpr std::string_view no_root = ":set root= でルートを決めてください";

// 絵の寸法（DIP・採用案 B の表）。線の太さ 1.25 DIP、升 16 DIP の座標で書いた絵、ファイルの印は
// 6 DIP の四角で角の半径 1.5 DIP。絵の細部（点の位置）は採用案のモックの SVG を升へ写したもの。
constexpr float stroke_dips = 1.25F;
constexpr float cell_units = 16.0F;
constexpr float file_mark_dips = 6.0F;
constexpr float file_mark_radius_dips = 1.5F;

// 山形（閉 `>`・開 `v`）。
constexpr std::array<D2D1_POINT_2F, 3> closed_chevron{{{6.5F, 5.0F}, {9.5F, 8.0F}, {6.5F, 11.0F}}};
constexpr std::array<D2D1_POINT_2F, 3> open_chevron{{{5.0F, 6.5F}, {8.0F, 9.5F}, {11.0F, 6.5F}}};
// フォルダ（閉は輪郭の 1 周・開は背の折れ線と前の面の 1 周）。
constexpr std::array<D2D1_POINT_2F, 6> closed_folder{
    {{2.5F, 4.0F}, {6.1F, 4.0F}, {7.3F, 5.5F}, {13.5F, 5.5F}, {13.5F, 12.5F}, {2.5F, 12.5F}}};
constexpr std::array<D2D1_POINT_2F, 6> open_folder_back{
    {{2.5F, 12.5F}, {2.5F, 4.0F}, {6.1F, 4.0F}, {7.3F, 5.5F}, {13.5F, 5.5F}, {13.5F, 7.1F}}};
constexpr std::array<D2D1_POINT_2F, 4> open_folder_front{
    {{2.5F, 12.5F}, {4.3F, 7.1F}, {14.2F, 7.1F}, {12.4F, 12.5F}}};

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}

[[nodiscard]] D2D1_RECT_F to_rect(const core::LayoutRect &area) noexcept
{
    return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                       static_cast<float>(area.right), static_cast<float>(area.bottom));
}

[[nodiscard]] std::string_view root_name(const core::FilePath &root) noexcept
{
    const std::string_view name = root.file_name();
    return name.empty() ? root.text() : name;
}

// 升 cell の中の絵の点（升 16 の座標）を物理画素へ写す。
[[nodiscard]] D2D1_POINT_2F placed(const core::LayoutRect &cell, D2D1_POINT_2F point) noexcept
{
    const float unit = static_cast<float>(core::height_of(cell)) / cell_units;
    return D2D1::Point2F(static_cast<float>(cell.left) + (point.x * unit),
                         static_cast<float>(cell.top) + (point.y * unit));
}

// 升 cell の中央に置いた辺 side の正方形。
[[nodiscard]] D2D1_RECT_F centered(const core::LayoutRect &cell, float side) noexcept
{
    const float left =
        static_cast<float>(cell.left) + ((static_cast<float>(core::width_of(cell)) - side) / 2.0F);
    const float top =
        static_cast<float>(cell.top) + ((static_cast<float>(core::height_of(cell)) - side) / 2.0F);
    return D2D1::RectF(left, top, left + side, top + side);
}
} // namespace

ExplorerPainter::ExplorerPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                                 const ExplorerText &text, PaletteRowLayouts &layouts,
                                 ExplorerShapes &shapes) noexcept
    : target_(target), brush_(brush), text_(text), layouts_(layouts), shapes_(shapes)
{
}

void ExplorerPainter::paint(const application::ExplorerView &view,
                            const core::ExplorerLayout &layout, const core::Palette &palette,
                            const core::SyntaxPalette &syntax)
{
    if (!layout.panel.has_value())
    {
        return;
    }
    const core::LayoutRect panel = layout.panel.value();
    if (core::width_of(panel) <= 0 || core::height_of(panel) <= 0 || layout.line_height <= 0)
    {
        return;
    }
    palette_ = palette;
    syntax_ = syntax;
    target_.GetFactory(factory_.ReleaseAndGetAddressOf());
    stroke_ = factory_ == nullptr ? nullptr : shapes_.stroke(*factory_.Get());
    fill(panel, palette.title_bar);
    target_.PushAxisAlignedClip(to_rect(panel), D2D1_ANTIALIAS_MODE_ALIASED);
    paint_root(view, layout);
    for (std::size_t index = 0; index < view.rows.size(); ++index)
    {
        if (core::explorer_line(layout, index + 1).top >= panel.bottom)
        {
            break;
        }
        paint_row(view, index, layout);
    }
    const core::RgbColor edge = view.focused ? palette.accent : palette.panel_border;
    // 境界は面の上端から下端まで直線（本文の帯の左上の角は直角・#1488）。
    fill(core::LayoutRect{std::max(panel.left, panel.right - layout.border), panel.top, panel.right,
                          panel.bottom},
         edge);
    target_.PopAxisAlignedClip();
}

void ExplorerPainter::fill(const core::LayoutRect &area, core::RgbColor color)
{
    brush_.SetColor(to_color(color));
    target_.FillRectangle(to_rect(area), &brush_);
}

void ExplorerPainter::fill_round(const core::LayoutRect &area, float radius, core::RgbColor color)
{
    brush_.SetColor(to_color(color));
    target_.FillRoundedRectangle(D2D1::RoundedRect(to_rect(area), radius, radius), &brush_);
}

void ExplorerPainter::write(std::string_view text, const core::LayoutRect &area,
                            core::RgbColor color, IDWriteTextFormat *format)
{
    const auto shown =
        layouts_.obtain(PaletteRowKey{format, PaletteTextRole::plain, text, area, text_.dpi},
                        [&] { return made(text, area, format); });
    if (!shown)
    {
        return;
    }
    brush_.SetColor(to_color(color));
    target_.DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)), shown.Get(),
        &brush_, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void ExplorerPainter::paint_root(const application::ExplorerView &view,
                                 const core::ExplorerLayout &layout)
{
    const core::Palette &palette = palette_;
    const core::LayoutRect area = core::explorer_root_name(layout, core::explorer_line(layout, 0));
    if (!view.root.has_value())
    {
        write(no_root, area, palette.muted, text_.format);
        return;
    }
    write(root_name(view.root.value()), area, palette.text, text_.strong);
}

void ExplorerPainter::paint_row(const application::ExplorerView &view, std::size_t index,
                                const core::ExplorerLayout &layout)
{
    const core::Palette &palette = palette_;
    const application::ExplorerRowView &row = view.rows.at(index);
    const core::LayoutRect line = core::explorer_line(layout, index + 1);
    const auto parts = core::explorer_row_parts(layout, line, row.depth, row.unsaved);
    paint_surface(view, index, parts.surface, layout);
    // 字下げの縦の線。深さ level の山形の升の中央を、それより深い行に通す。深さは面の幅を超えて
    // 数えない（explorer_row_parts と同じ切り詰め）。
    const std::size_t levels = std::min<std::size_t>(
        row.depth, static_cast<std::size_t>(std::max(core::width_of(line), 0)));
    for (std::size_t level = 0; level < levels; ++level)
    {
        const std::int32_t x = core::explorer_guide_x(layout, line, level);
        fill(core::LayoutRect{x, line.top, x + layout.border, line.bottom}, palette.panel_border);
    }
    if (row.more)
    {
        write(more_mark, parts.name, palette.muted, text_.format);
        return;
    }
    paint_lead(row, parts.chevron, parts.mark);
    write(row.name, parts.name, row.hidden ? palette.muted : palette.text,
          row.active ? text_.strong : text_.format);
    if (row.unsaved)
    {
        const auto radius = static_cast<float>(core::width_of(parts.dot)) / 2.0F;
        brush_.SetColor(to_color(palette.accent));
        target_.FillEllipse(D2D1::Ellipse(D2D1::Point2F(static_cast<float>(parts.dot.left) + radius,
                                                        static_cast<float>(parts.dot.top) + radius),
                                          radius, radius),
                            &brush_);
    }
}

// 行の丸い面。選んだ行は本文の色に 1 DIP の枠（焦点があれば accent・無ければ panel_border。背景が
// 黒のテーマでは面の地と本文の色が同じなので、枠が無いと見えない・V-1428 の F-1）、載せた行は
// tab_hover（選んだ行には重ねない）。ほかの行は面の地のまま。
void ExplorerPainter::paint_surface(const application::ExplorerView &view, std::size_t index,
                                    const core::LayoutRect &surface,
                                    const core::ExplorerLayout &layout)
{
    const core::Palette &palette = palette_;
    const std::size_t row = view.first_visible + index;
    const auto radius = static_cast<float>(layout.radius);
    if (view.selected == row)
    {
        fill_round(surface, radius, palette.background);
        const auto half = static_cast<float>(layout.border) / 2.0F;
        const auto area = to_rect(surface);
        brush_.SetColor(to_color(view.focused ? palette.accent : palette.panel_border));
        target_.DrawRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(area.left + half, area.top + half, area.right - half,
                                          area.bottom - half),
                              radius, radius),
            &brush_, static_cast<float>(layout.border));
        return;
    }
    if (view.hovered == row)
    {
        fill_round(surface, radius, palette.tab_hover);
    }
}

// 山形とフォルダの絵（フォルダの行）、またはファイルの印（ファイルの行・山形の升は空ける）。
void ExplorerPainter::paint_lead(const application::ExplorerRowView &row,
                                 const core::LayoutRect &chevron, const core::LayoutRect &mark)
{
    const core::Palette &palette = palette_;
    if (row.folder)
    {
        stroke_lines(chevron, row.open ? open_chevron : closed_chevron, false, palette.muted);
        if (row.open)
        {
            stroke_lines(mark, open_folder_back, false, palette.muted);
            stroke_lines(mark, open_folder_front, true, palette.muted);
            return;
        }
        stroke_lines(mark, closed_folder, true, palette.muted);
        return;
    }
    const core::RgbColor color =
        row.mark.has_value() ? core::syntax_color(syntax_, row.mark.value()) : palette.muted;
    const float radius = file_mark_radius_dips * scale();
    brush_.SetColor(to_color(color));
    target_.FillRoundedRectangle(
        D2D1::RoundedRect(centered(mark, file_mark_dips * scale()), radius, radius), &brush_);
}

// 升 cell の中に点の列を線でつなぐ（closed なら最後の点から最初の点へも）。
void ExplorerPainter::stroke_lines(const core::LayoutRect &cell,
                                   std::span<const D2D1_POINT_2F> points, bool closed,
                                   core::RgbColor color)
{
    if (core::height_of(cell) <= 0 || core::width_of(cell) <= 0 || points.size() < 2)
    {
        return;
    }
    brush_.SetColor(to_color(color));
    const float width = stroke_dips * scale();
    for (std::size_t at = 0; at + 1 < points.size(); ++at)
    {
        target_.DrawLine(placed(cell, points[at]), placed(cell, points[at + 1]), &brush_, width,
                         stroke_);
    }
    if (closed)
    {
        target_.DrawLine(placed(cell, points.back()), placed(cell, points.front()), &brush_, width,
                         stroke_);
    }
}

float ExplorerPainter::scale() const noexcept
{
    return static_cast<float>(text_.dpi) / static_cast<float>(core::reference_dpi);
}

PaletteRowLayouts::TextLayout ExplorerPainter::made(std::string_view text,
                                                    const core::LayoutRect &area,
                                                    IDWriteTextFormat *format)
{
    const std::wstring wide = core::to_utf16(text).value_or(std::wstring{});
    PaletteRowLayouts::TextLayout layout;
    // 折り返さず幅で切る。書式の側に NO_WRAP と文字単位の省略記号を立ててある。
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
