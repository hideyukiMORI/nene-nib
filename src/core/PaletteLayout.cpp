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
    const auto count = std::min({std::max(choices, std::size_t{1}), available, std::size_t{8}});
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

std::size_t palette_first_visible(const PaletteLayout &layout, std::size_t selected) noexcept
{
    if (layout.visible_rows == 0)
    {
        return 0;
    }
    return std::max(selected + 1, layout.visible_rows) - layout.visible_rows;
}
} // namespace nenenib::core
