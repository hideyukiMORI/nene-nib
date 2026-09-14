#pragma once

#include <cstdint>

namespace nenenib::core
{
// 物理画素の矩形。4 つの辺は互いに独立して妥当なので公開 aggregate（ADR 0007）。
// right / bottom は半開区間の端（その画素は含まない）。
struct LayoutRect
{
    std::int32_t left;
    std::int32_t top;
    std::int32_t right;
    std::int32_t bottom;
};

[[nodiscard]] constexpr bool operator==(const LayoutRect &left, const LayoutRect &right) noexcept
{
    return left.left == right.left && left.top == right.top && left.right == right.right &&
           left.bottom == right.bottom;
}

[[nodiscard]] constexpr std::int32_t width_of(const LayoutRect &rectangle) noexcept
{
    return rectangle.right - rectangle.left;
}

[[nodiscard]] constexpr std::int32_t height_of(const LayoutRect &rectangle) noexcept
{
    return rectangle.bottom - rectangle.top;
}

[[nodiscard]] constexpr bool contains(const LayoutRect &rectangle, std::int32_t x,
                                      std::int32_t y) noexcept
{
    return x >= rectangle.left && x < rectangle.right && y >= rectangle.top && y < rectangle.bottom;
}
} // namespace nenenib::core
