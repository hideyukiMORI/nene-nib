#pragma once

#include "SplitShare.hpp"

#include <cstdint>

namespace nenenib::ui::win32
{
// 2 枠の境界を押してから離すまでの覚え（ADR 0148 の 2026-11-22 の追補の決定 3）。座標は分割の向き
// （左右なら x・上下なら y）の client の物理画素。offset は押した点と境界の始まりのずれ、at
// は押した 点（同じ点の WM_MOUSEMOVE を動かしたと数えない）、dragged
// は押した点から動いたか、pressed は押す 前の比（Esc で戻す）。どの欄も単独で妥当な値なので公開
// aggregate（CPP-003）。
struct SplitGrab
{
    std::int32_t offset;
    std::int32_t at;
    bool dragged;
    core::SplitShare pressed;
};

[[nodiscard]] constexpr bool operator==(const SplitGrab &left, const SplitGrab &right) noexcept
{
    return left.offset == right.offset && left.at == right.at && left.dragged == right.dragged &&
           left.pressed == right.pressed;
}
} // namespace nenenib::ui::win32
