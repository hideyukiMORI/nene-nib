#pragma once

#include <cstdint>

namespace nenenib::core
{
inline constexpr std::uint32_t reference_dpi = 96;

// DIP を物理画素へ。四捨五入は整数だけで行う（std::lround は core が持てない libm の
// シンボルを残す・ARC-003）。寸法は非負なので std::lround と同じ値になる。
[[nodiscard]] constexpr std::int32_t to_pixels(std::int32_t dips, std::uint32_t dpi) noexcept
{
    const auto reference = static_cast<std::int32_t>(reference_dpi);
    return (dips * static_cast<std::int32_t>(dpi) + reference / 2) / reference;
}
} // namespace nenenib::core
