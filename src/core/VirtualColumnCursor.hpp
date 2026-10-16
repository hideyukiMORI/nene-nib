#pragma once

#include <cstddef>

namespace nenenib::core
{
// 本文の位置の手前までの累積（ADR 0140 の決定 4）。column は行頭からの仮想桁の和、cell は格子の
// 升の和（送りの空きを含む・text の索引では column と同じ）。scanned は、この値を出すために
// 文字を読んで歩いた塊の数（0 か 1。plain の塊は割り算で歩かない）。
struct VirtualColumnCursor
{
    std::size_t column;
    std::size_t cell;
    std::size_t scanned;
};
} // namespace nenenib::core
