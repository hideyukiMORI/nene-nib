#pragma once

#include <cstddef>

namespace nenenib::application
{
// 格子の 1 画面行の中の升の範囲 [begin, end)（ADR 0140 の決定 5）。左端からの升で数える。
struct GridCellSpan
{
    std::size_t begin;
    std::size_t end;
};

[[nodiscard]] constexpr bool operator==(GridCellSpan left, GridCellSpan right) noexcept
{
    return left.begin == right.begin && left.end == right.end;
}
} // namespace nenenib::application
