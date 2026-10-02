#include "PaletteLayout.hpp"

#include "DevicePixels.hpp"

#include <algorithm>

namespace nenenib::core
{
PaletteLayout palette_layout(std::int32_t width, std::int32_t height, std::uint32_t dpi,
                             std::size_t choices) noexcept
{
    const auto margin = to_pixels(16, dpi);
    const auto panel_width = std::max(std::min(to_pixels(640, dpi), width - margin * 2), 0);
    const auto left = std::max((width - panel_width) / 2, 0);
    const auto body_top = std::min(to_pixels(52, dpi), height);
    const auto bottom = std::max(body_top, height - to_pixels(28, dpi) - margin);
    const auto top = std::min(to_pixels(96, dpi), std::max(body_top, bottom - to_pixels(86, dpi)));
    const auto header_height = std::min(to_pixels(52, dpi), bottom - top);
    const auto footer_height = std::min(to_pixels(34, dpi), bottom - top - header_height);
    const auto row_height = std::max(to_pixels(40, dpi), 1);
    const auto available =
        static_cast<std::size_t>((bottom - top - header_height - footer_height) / row_height);
    const auto count = std::min({std::max(choices, std::size_t{1}), available, palette_row_limit});
    const auto rows_top = top + header_height;
    const auto rows_bottom = rows_top + static_cast<std::int32_t>(count) * row_height;
    const auto right = left + panel_width;
    const auto input_left = std::min(left + margin, right);
    const auto input_right = std::max(input_left, right - margin);
    return PaletteLayout{
        LayoutRect{left, top, right, rows_bottom + footer_height},
        LayoutRect{input_left, top, input_right, rows_top},
        LayoutRect{left, rows_top, right, rows_bottom},
        LayoutRect{input_left, rows_bottom, input_right, rows_bottom + footer_height},
        row_height,
        count};
}

LayoutRect palette_row(const PaletteLayout &layout, std::size_t visible_index) noexcept
{
    const auto at = std::min(visible_index, layout.visible_rows);
    const auto top = layout.rows.top + static_cast<std::int32_t>(at) * layout.row_height;
    return LayoutRect{layout.rows.left, top, layout.rows.right,
                      std::min(top + layout.row_height, layout.rows.bottom)};
}

std::optional<std::size_t> palette_hit(const PaletteLayout &layout, std::int32_t x,
                                       std::int32_t y) noexcept
{
    if (!contains(layout.rows, x, y))
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>((y - layout.rows.top) / layout.row_height);
}

namespace
{
// 見えている行数 visible で選択 selected を最後の行に収める先頭。描く行と frame の窓が共有する
// 1 つの式（ARC-001）。
std::size_t first_row_of(std::size_t visible, std::size_t selected) noexcept
{
    if (visible == 0)
    {
        return 0;
    }
    return std::max(selected + 1, visible) - visible;
}
} // namespace

std::size_t palette_first_visible(const PaletteLayout &layout, std::size_t selected) noexcept
{
    return first_row_of(layout.visible_rows, selected);
}

std::size_t palette_window_first(std::size_t selected) noexcept
{
    return first_row_of(palette_row_limit, selected);
}

namespace
{
// 行の内側。選択の面の内側 8 DIP のさらに 8 DIP 内（今までの描画と 1 画素も変えないため 8 DIP を
// 2 回足す）。
LayoutRect row_inside(const LayoutRect &row, std::uint32_t dpi) noexcept
{
    const auto inset = to_pixels(8, dpi) * 2;
    const auto left = std::min(row.left + inset, row.right);
    return LayoutRect{left, row.top, std::max(left, row.right - inset), row.bottom};
}
} // namespace

LayoutRect palette_row_note(const LayoutRect &row, std::uint32_t dpi) noexcept
{
    const auto inside = row_inside(row, dpi);
    if (width_of(inside) < to_pixels(112 + 12 + 120, dpi))
    {
        return LayoutRect{inside.right, inside.top, inside.right, inside.bottom};
    }
    return LayoutRect{inside.right - to_pixels(112, dpi), inside.top, inside.right, inside.bottom};
}

LayoutRect palette_row_label(const LayoutRect &row, std::uint32_t dpi, bool noted) noexcept
{
    const auto inside = row_inside(row, dpi);
    const auto note = palette_row_note(row, dpi);
    if (!noted || width_of(note) <= 0)
    {
        return inside;
    }
    return LayoutRect{inside.left, inside.top, note.left - to_pixels(12, dpi), inside.bottom};
}

LayoutRect palette_input_hint(const PaletteLayout &layout) noexcept
{
    // 行の高さは 40 DIP なので、160 DIP はその 4 つぶん（配置は dpi を持たない）。
    const auto least = layout.row_height * 4;
    const auto &input = layout.input;
    const auto centre = input.left + width_of(input) / 2;
    if (input.right - centre < least)
    {
        return LayoutRect{input.right, input.top, input.right, input.bottom};
    }
    return LayoutRect{centre, input.top, input.right, input.bottom};
}
} // namespace nenenib::core
