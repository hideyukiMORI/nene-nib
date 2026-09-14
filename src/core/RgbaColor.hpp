#pragma once

#include "RgbColor.hpp"

#include <cstdint>

namespace nenenib::core
{
// 不透明度を持つ色。色と alpha は互いに独立して妥当なので公開 aggregate（ADR 0007）。
struct RgbaColor
{
    RgbColor color;
    std::uint8_t alpha;
};

// 等値は非メンバで書く（CPP-003・Issue #3 で実測した T1-tidy-public-with-method）。
[[nodiscard]] constexpr bool operator==(const RgbaColor &left, const RgbaColor &right) noexcept
{
    return left.color == right.color && left.alpha == right.alpha;
}
} // namespace nenenib::core
