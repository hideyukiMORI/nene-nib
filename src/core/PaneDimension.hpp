#pragma once

#include <cstdint>

namespace nenenib::core
{
// 枠の大きさのどちらの向きか（ADR 0148 の 2026-11-22 の追補・#1941）。左右の分割（vertical）で
// 動くのは width、上下の分割（horizontal）で動くのは height。値を足したら落ちた switch を直す
// （CPP-002）。
enum class PaneDimension : std::uint8_t
{
    width,
    height
};
} // namespace nenenib::core
