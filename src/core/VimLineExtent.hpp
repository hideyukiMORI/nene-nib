#pragma once

#include "VimColumnWish.hpp"

#include <cstddef>

namespace nenenib::core
{
// 行単位 VISUAL で行った変更の「範囲の大きさ」（ADR 0033 の決定 5）。行数だけで、桁は持たない
// （Vim も行単位の再生では同じ行数だけを使う・Issue #91 で実測）。ただし `$` で選んだかは覚える
// （Vim の rv_vcol == MAXCOL・再生は coladvance(MAXCOL) で行の最後の文字へ置く・`v$AX<Esc>j0.` は
// 行末の文字の後ろ・#1436）。
struct VimLineExtent
{
    std::size_t lines;
    VimColumnWish wish = VimColumnWish::at_column;
};

[[nodiscard]] constexpr bool operator==(VimLineExtent left, VimLineExtent right) noexcept
{
    return left.lines == right.lines && left.wish == right.wish;
}
} // namespace nenenib::core
