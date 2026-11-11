#pragma once

#include "VirtualColumn.hpp"

namespace nenenib::core
{
// VISUAL のオペレータが最後に覚えた範囲の仮想桁（Vim の start_vcol と end_vcol・#1436）。
// Vim の do_pending_operator は、`$` でないとき終わりの最後の桁を書き直し、
// 1 行だけなら始まりの最初の桁も書き直す。ほかは前のオペレータの値を残す（再生の中も）。
// 文字単位と行単位の VISUAL の `A` は、2 つが違うときだけ終わりの文字の後ろに打つ
// （op_insert の inc_cursor・実測）。初めは両方とも 1 桁目（Vim の 0）。
struct VimOperatorColumns
{
    VirtualColumn start{1};
    VirtualColumn end{1};
};

[[nodiscard]] constexpr bool operator==(const VimOperatorColumns &left,
                                        const VimOperatorColumns &right) noexcept
{
    return left.start == right.start && left.end == right.end;
}
} // namespace nenenib::core
