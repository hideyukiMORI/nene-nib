#include "ScrollBounds.hpp"

#include <algorithm>

namespace nenenib::core
{
namespace
{
[[nodiscard]] std::size_t last_first_visible(std::size_t total_lines,
                                             std::size_t visible_lines) noexcept
{
    const std::size_t lines = std::max<std::size_t>(visible_lines, 1);
    return total_lines > lines ? total_lines - lines + 1 : 1;
}
} // namespace

LineNumber first_visible_within(LineNumber requested, std::size_t total_lines,
                                std::size_t visible_lines) noexcept
{
    const std::size_t highest = last_first_visible(total_lines, visible_lines);
    return LineNumber{std::clamp<std::size_t>(requested.value, 1, highest)};
}

LineNumber first_visible_for_caret(LineNumber first_visible, LineNumber caret_line,
                                   std::size_t visible_lines) noexcept
{
    const std::size_t lines = std::max<std::size_t>(visible_lines, 1);
    if (caret_line.value < first_visible.value)
    {
        return caret_line;
    }
    if (caret_line.value >= first_visible.value + lines)
    {
        return LineNumber{caret_line.value - lines + 1};
    }
    return first_visible;
}
} // namespace nenenib::core
