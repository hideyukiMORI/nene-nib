#include "ScrollBounds.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] std::size_t last_first_visible(std::size_t total_lines, std::size_t visible_lines,
                                             ScrollExtent extent) noexcept
{
    switch (extent)
    {
    case ScrollExtent::filled_viewport:
    {
        const std::size_t lines = std::max<std::size_t>(visible_lines, 1);
        return total_lines > lines ? total_lines - lines + 1 : 1;
    }
    case ScrollExtent::last_line:
        return std::max<std::size_t>(total_lines, 1);
    }
    std::unreachable();
}

[[nodiscard]] bool should_recenter(ScrollFollow follow, std::size_t outside,
                                   std::size_t threshold) noexcept
{
    switch (follow)
    {
    case ScrollFollow::minimal:
        return false;
    case ScrollFollow::vim:
        return outside >= threshold;
    }
    std::unreachable();
}

[[nodiscard]] std::size_t upper_threshold(std::size_t lines) noexcept
{
    return lines > 2 ? (lines - 2) / 2 : 0;
}

[[nodiscard]] LineNumber shifted_above(LineNumber line, std::size_t amount) noexcept
{
    return LineNumber{line.value > amount ? line.value - amount : 1};
}

[[nodiscard]] LineNumber followed_above(LineNumber first_visible, LineNumber caret_line,
                                        std::size_t lines, ScrollFollow follow) noexcept
{
    const std::size_t outside = first_visible.value - caret_line.value;
    if (should_recenter(follow, outside, upper_threshold(lines)))
    {
        return shifted_above(caret_line, (lines - 1) / 2);
    }
    return caret_line;
}

[[nodiscard]] LineNumber followed_below(LineNumber first_visible, LineNumber caret_line,
                                        std::size_t lines, ScrollFollow follow) noexcept
{
    const std::size_t offset = caret_line.value - first_visible.value;
    const std::size_t outside = offset - lines + 1;
    const std::size_t lower_threshold = lines - upper_threshold(lines);
    if (should_recenter(follow, outside, lower_threshold))
    {
        return shifted_above(caret_line, lines / 2);
    }
    return LineNumber{caret_line.value - lines + 1};
}
} // namespace

LineNumber first_visible_within(LineNumber requested, std::size_t total_lines,
                                std::size_t visible_lines, ScrollExtent extent) noexcept
{
    const std::size_t highest = last_first_visible(total_lines, visible_lines, extent);
    return LineNumber{std::clamp<std::size_t>(requested.value, 1, highest)};
}

LineNumber first_visible_for_caret(LineNumber first_visible, LineNumber caret_line,
                                   std::size_t visible_lines, ScrollFollow follow) noexcept
{
    const std::size_t lines = std::max<std::size_t>(visible_lines, 1);
    if (caret_line.value < first_visible.value)
    {
        return followed_above(first_visible, caret_line, lines, follow);
    }
    const std::size_t offset = caret_line.value - first_visible.value;
    if (offset >= lines)
    {
        return followed_below(first_visible, caret_line, lines, follow);
    }
    return first_visible;
}
} // namespace nenenib::core
