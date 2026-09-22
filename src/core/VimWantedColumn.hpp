#pragma once

#include "VimColumnWish.hpp"
#include "VirtualColumn.hpp"

namespace nenenib::core
{
// 欲しい列（Vim の curswant・ADR 0012 の決定 1）。at_line_end のとき column は見ない。
// 桁は仮想桁（表示幅で数える）で、Tab と全角の混ざる本文でも固定 Vim と同じ所へ着く
// （ADR 0034 の決定 3）。
struct VimWantedColumn
{
    VimColumnWish wish;
    VirtualColumn column;
};

[[nodiscard]] constexpr bool operator==(const VimWantedColumn &left,
                                        const VimWantedColumn &right) noexcept
{
    return left.wish == right.wish && left.column == right.column;
}
} // namespace nenenib::core
