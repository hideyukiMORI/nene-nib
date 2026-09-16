#pragma once

#include "Column.hpp"
#include "VimColumnWish.hpp"

namespace nenenib::core
{
// 欲しい列（Vim の curswant・ADR 0012 の決定 1）。at_line_end のとき column は見ない。
struct VimWantedColumn
{
    VimColumnWish wish;
    Column column;
};

[[nodiscard]] constexpr bool operator==(const VimWantedColumn &left,
                                        const VimWantedColumn &right) noexcept
{
    return left.wish == right.wish && left.column == right.column;
}
} // namespace nenenib::core
