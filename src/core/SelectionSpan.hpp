#pragma once

#include "Column.hpp"
#include "SelectionPresence.hpp"

namespace nenenib::core
{
// 表示する 1 行の選択の面。桁の半開区間 [begin, end) で、有無は presence が持つ（CPP-004）。
struct SelectionSpan
{
    SelectionPresence presence;
    Column begin;
    Column end;
};

[[nodiscard]] constexpr bool operator==(const SelectionSpan &left,
                                        const SelectionSpan &right) noexcept
{
    return left.presence == right.presence && left.begin == right.begin && left.end == right.end;
}

[[nodiscard]] constexpr SelectionSpan no_selection_span() noexcept
{
    return SelectionSpan{SelectionPresence::absent, Column{1}, Column{1}};
}
} // namespace nenenib::core
