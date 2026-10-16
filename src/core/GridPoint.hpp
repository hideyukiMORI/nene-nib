#pragma once

#include <cstddef>

namespace nenenib::core
{
// 格子の上の位置。row は行の中の画面行（0 始まり）、cell はその画面行の左端からの升（0〜columns）。
struct GridPoint
{
    std::size_t row;
    std::size_t cell;
};
} // namespace nenenib::core
