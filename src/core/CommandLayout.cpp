#include "CommandLayout.hpp"

#include "DevicePixels.hpp"

#include <algorithm>

namespace nenenib::core
{
CommandLayout command_layout(const StatusBarLayout &status, std::uint32_t dpi,
                             std::size_t candidates) noexcept
{
    const auto left = std::min(status.toggle.left, status.band.right);
    const auto right = std::max(left, status.items.front().left - to_pixels(16, dpi));
    const auto row = std::max(to_pixels(24, dpi), 1);
    const auto available = std::max(status.band.top - to_pixels(80, dpi), 0) / row;
    const auto rows =
        std::min({candidates, static_cast<std::size_t>(available), command_completion_row_limit});
    return CommandLayout{LayoutRect{left, status.band.top, right, status.band.bottom},
                         LayoutRect{left, status.band.top - static_cast<std::int32_t>(rows) * row,
                                    right, status.band.top},
                         row, rows};
}

LayoutRect command_row_label(const LayoutRect &row, std::uint32_t dpi) noexcept
{
    const auto inset = to_pixels(command_text_inset_dip, dpi);
    const auto left = row.left + inset;
    return LayoutRect{left, row.top, std::max(left, row.right - inset), row.bottom};
}

LayoutRect command_input_label(const CommandLayout &layout, std::uint32_t dpi) noexcept
{
    return command_row_label(layout.input, dpi);
}
} // namespace nenenib::core
