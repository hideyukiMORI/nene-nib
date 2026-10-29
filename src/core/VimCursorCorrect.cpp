#include "VimCursorCorrect.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
namespace
{
// 上と下に欲しい行の数（Vim の above_wanted と below_wanted）。
using Wanted = std::pair<std::size_t, std::size_t>;

[[nodiscard]] Wanted wanted_of(const VimCursorWindow &window, std::size_t line_count) noexcept
{
    Wanted wanted{window.margin, window.margin};
    if (window.top.value <= 1)
    {
        wanted.first = 0;
        wanted.second = std::min(wanted.second, window.height / 2);
    }
    if (window.below.value > line_count)
    {
        wanted.second = 0;
        wanted.first = std::min(wanted.first, window.height > 0 ? (window.height - 1) / 2 : 0);
    }
    return wanted;
}

// 下の欲しい行をもう 1 行詰める番か（Vim の cursor_correct の狭め方・下を先に詰める）。
[[nodiscard]] bool narrows_below(Wanted wanted, std::size_t above, std::size_t below) noexcept
{
    return below < wanted.second && (below <= above || above >= wanted.first);
}

[[nodiscard]] bool narrows_above(Wanted wanted, std::size_t above, std::size_t below) noexcept
{
    return above < wanted.first && (above < below || below >= wanted.second);
}

// 窓の両端から欲しい分だけ 1 行ずつ詰めた範囲（Vim の topline と botline の狭め方）。
[[nodiscard]] std::pair<std::size_t, std::size_t> narrowed(const VimCursorWindow &window,
                                                           Wanted wanted) noexcept
{
    std::size_t top = window.top.value;
    std::size_t bottom = window.below.value > 0 ? window.below.value - 1 : 0;
    std::size_t above = 0;
    std::size_t below = 0;
    while ((above < wanted.first || below < wanted.second) && top < bottom)
    {
        if (narrows_below(wanted, above, below))
        {
            ++below;
            --bottom;
        }
        if (narrows_above(wanted, above, below))
        {
            ++above;
            ++top;
        }
    }
    return {top, bottom};
}

// 詰めた範囲 [top, bottom] へ寄せる。範囲が潰れたら残った行。窓の先頭が 1 行目なら上へは寄せず、
// 本文の末尾まで見えていれば下へは寄せない。
[[nodiscard]] LineNumber placed_within(const VimCursorWindow &window, LineNumber caret,
                                       std::pair<std::size_t, std::size_t> range,
                                       std::size_t line_count) noexcept
{
    const auto [top, bottom] = range;
    if (top == bottom || bottom == 0)
    {
        return LineNumber{top};
    }
    if (top > bottom)
    {
        return LineNumber{bottom};
    }
    if (caret.value < top && window.top.value > 1)
    {
        return LineNumber{top};
    }
    if (caret.value > bottom && window.below.value <= line_count)
    {
        return LineNumber{bottom};
    }
    return caret;
}
} // namespace

LineNumber vim_cursor_corrected(const VimCursorWindow &window, LineNumber caret,
                                std::size_t line_count) noexcept
{
    const Wanted wanted = wanted_of(window, line_count);
    if (caret.value >= window.top.value + wanted.first &&
        caret.value + wanted.second < window.below.value)
    {
        return caret;
    }
    return placed_within(window, caret, narrowed(window, wanted), line_count);
}
} // namespace nenenib::core
