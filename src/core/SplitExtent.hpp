#pragma once

#include "PaneDimension.hpp"

#include <cstdint>

namespace nenenib::core
{
// 境界を動かすときに読む今の寸法（ADR 0148 の 2026-11-22 の追補・#1941）。どれも物理画素で、
// 分割の向き dimension に沿って測る。origin は帯の始まり（左か上の端）、available は境界を除いた
// 帯の長さ、first は今の first の枠（左か上）の長さ、minimum はどちらの枠にも残す最小の長さ。
// 作るのは split_extent の 1 本。どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct SplitExtent
{
    PaneDimension dimension;
    std::int32_t origin;
    std::int32_t available;
    std::int32_t first;
    std::int32_t minimum;
};

[[nodiscard]] constexpr bool operator==(const SplitExtent &left, const SplitExtent &right) noexcept
{
    return left.dimension == right.dimension && left.origin == right.origin &&
           left.available == right.available && left.first == right.first &&
           left.minimum == right.minimum;
}
} // namespace nenenib::core
