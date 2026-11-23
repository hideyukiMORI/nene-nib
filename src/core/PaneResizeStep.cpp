#include "PaneResizeStep.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
std::optional<std::uint8_t> pane_resize_step_of(std::string_view text) noexcept
{
    constexpr std::size_t digits_limit = 2;
    constexpr std::uint32_t decimal_base = 10;
    if (text.empty() || text.size() > digits_limit || (text.size() > 1 && text.front() == '0'))
    {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    for (const char digit : text)
    {
        if (digit < '0' || digit > '9')
        {
            return std::nullopt;
        }
        value = (value * decimal_base) + static_cast<std::uint32_t>(digit - '0');
    }
    if (value < pane_resize_step_minimum || value > pane_resize_step_maximum)
    {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(value);
}

std::uint8_t pane_resize_step_stepped(std::uint8_t cells, bool forward) noexcept
{
    const auto clamped =
        std::clamp<std::int32_t>(cells, pane_resize_step_minimum, pane_resize_step_maximum);
    const std::int32_t moved = forward ? clamped + 1 : clamped - 1;
    return static_cast<std::uint8_t>(
        std::clamp<std::int32_t>(moved, pane_resize_step_minimum, pane_resize_step_maximum));
}

std::string pane_resize_step_text(std::uint8_t cells)
{
    return std::to_string(cells);
}

std::int32_t vim_uncounted_pane_cells(std::uint8_t step, bool follows) noexcept
{
    return follows ? static_cast<std::int32_t>(step) : 1;
}
} // namespace nenenib::core
