#pragma once

#include "Column.hpp"
#include "VimColumnWish.hpp"

#include <cstddef>

namespace nenenib::core
{
// 文字単位 VISUAL で行った変更の「範囲の大きさ」（ADR 0033 の決定 5）。本文もキャレットも
// 持たない大きさだけの値で、`.` はここからキャレットの所に同じ大きさを選び直す。
//
// `lines` は範囲が覆う行数（1 以上）。`wish` が at_line_end のときは `$` で選んだ「行末まで」で
// `column` を見ない（Vim の curswant が MAXCOL のまま残るのと同じ・Issue #91 で実測）。
// at_column のとき `column` は 1 行なら桁の個数、複数行なら最終行の端の桁（絶対・実測）。
// 桁は code point 単位（Column）で、Vim が数える仮想桁とは Tab と全角で食い違う（ADR 0033）。
struct VimCharacterExtent
{
    std::size_t lines;
    VimColumnWish wish;
    Column column;
};

[[nodiscard]] constexpr bool operator==(const VimCharacterExtent &left,
                                        const VimCharacterExtent &right) noexcept
{
    return left.lines == right.lines && left.wish == right.wish && left.column == right.column;
}
} // namespace nenenib::core
