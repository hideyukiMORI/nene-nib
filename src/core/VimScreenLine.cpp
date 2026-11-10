#include "VimScreenLine.hpp"

namespace nenenib::core
{
LineNumber vim_screen_top_line(LineNumber first, std::size_t count, std::size_t line_count) noexcept
{
    const std::size_t below = count > 0 ? count - 1 : 0;
    if (first.value >= line_count || below >= line_count - first.value)
    {
        return LineNumber{line_count};
    }
    return LineNumber{first.value + below};
}

LineNumber vim_screen_bottom_line(LineNumber last, std::size_t count) noexcept
{
    const std::size_t above = count > 0 ? count - 1 : 0;
    if (above >= last.value)
    {
        return LineNumber{1};
    }
    return LineNumber{last.value - above};
}
} // namespace nenenib::core
