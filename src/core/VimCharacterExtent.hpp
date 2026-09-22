#pragma once

#include "VimColumnWish.hpp"
#include "VirtualColumn.hpp"

#include <cstddef>

namespace nenenib::core
{
// 文字単位 VISUAL で行った変更の「範囲の大きさ」（ADR 0033 の決定 5）。本文もキャレットも
// 持たない大きさだけの値で、`.` はここからキャレットの所に同じ大きさを選び直す。
//
// `lines` は範囲が覆う行数（1 以上）。`wish` が at_line_end のときは `$` で選んだ「行末まで」で
// `column` を見ない（Vim の curswant が MAXCOL のまま残るのと同じ・Issue #91 で実測）。
// at_column のとき `column` は 1 行なら桁の個数、複数行なら最終行の端の桁（絶対・実測）。
// 桁は仮想桁（表示幅で数える）で、1 行の個数は範囲の先頭の最初の桁から末尾の最後の桁まで
// （`ab<Tab>cd` の `vll` は 8）、複数行は最終行の末尾の最後の桁（ADR 0034 の決定 4・実測）。
struct VimCharacterExtent
{
    std::size_t lines;
    VimColumnWish wish;
    VirtualColumn column;
};

[[nodiscard]] constexpr bool operator==(const VimCharacterExtent &left,
                                        const VimCharacterExtent &right) noexcept
{
    return left.lines == right.lines && left.wish == right.wish && left.column == right.column;
}
} // namespace nenenib::core
