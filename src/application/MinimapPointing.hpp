#pragma once

#include <cstdint>

namespace nenenib::application
{
// ミニマップの面の上の左ボタン（ADR 0171 の決定 5 の追補）。pressed は面の上で押した（押した所へ
// 跳ぶ）、dragged は押したまま動かした（離すまで追従）。離す・捕まえを失う・Esc は ui がドラッグを
// 終えるだけで、意図は送らない（窓はその時点の位置のまま）。閉じた集合（CPP-002）。
enum class MinimapPointing : std::uint8_t
{
    pressed,
    dragged
};
} // namespace nenenib::application
