#pragma once

#include "CaretPoint.hpp"

#include <array>

namespace nenenib::core
{
// キャレットの四隅（ADR 0192 の決定 2）。並びは左上・右上・右下・左下で、止まっているときは矩形、
// 動いている間は 4 点の多角形。公開 aggregate（ADR 0007）。
struct CaretQuad
{
    std::array<CaretPoint, 4> corners;
};

[[nodiscard]] constexpr bool operator==(const CaretQuad &left, const CaretQuad &right) noexcept
{
    return left.corners == right.corners;
}
} // namespace nenenib::core
