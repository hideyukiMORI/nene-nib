#pragma once

#include <cstddef>

namespace nenenib::core
{
// 格子の上の位置（ADR 0140 の決定 4）。row は行の中の画面行（0 始まり）、cell はその画面行の
// 左端からの升（0〜columns）。cell が columns になるのは、行の終わりが画面行の右端に立つときだけ。
struct GridPoint
{
    std::size_t row;
    std::size_t cell;
};

[[nodiscard]] constexpr bool operator==(GridPoint left, GridPoint right) noexcept
{
    return left.row == right.row && left.cell == right.cell;
}
} // namespace nenenib::core
