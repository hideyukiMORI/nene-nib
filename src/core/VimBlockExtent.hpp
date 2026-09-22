#pragma once

#include "VimBlockWidth.hpp"
#include "VimColumnWish.hpp"

#include <cstddef>

namespace nenenib::core
{
// 矩形 VISUAL で行った変更の「範囲の大きさ」（ADR 0035 の決定 7）。行数と桁数だけで、本文も
// キャレットも持たない。`wish` が at_line_end なら `$` で取った矩形で、幅ではなく「各行の
// 内容の終わりまで」を繰り返す（固定 Vim も curswant を MAXCOL のまま覚え直す・実測）。
struct VimBlockExtent
{
    std::size_t lines;
    VimColumnWish wish;
    VimBlockWidth width;
};

[[nodiscard]] constexpr bool operator==(const VimBlockExtent &left,
                                        const VimBlockExtent &right) noexcept
{
    return left.lines == right.lines && left.wish == right.wish && left.width == right.width;
}
} // namespace nenenib::core
