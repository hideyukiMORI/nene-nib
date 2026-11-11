#pragma once

#include "MinimapPointing.hpp"

#include <cstdint>

namespace nenenib::application
{
// ミニマップの面の上のマウス（ADR 0171 の決定 5 の追補）。row は面の段（0 始まり・
// core::minimap_pointed_row で点を直した値・ドラッグで面の外へ出れば負か面の段の数以上）。どの行
// を見せるかは application が core::minimap_pointed_first の 1 本で決め、本文の窓だけを送る
// （キャレットと選択は動かさない・ホイールの ScrollLines と同じ送りと収め方）。
struct PointMinimap
{
    MinimapPointing pointing;
    std::int32_t row;
};
} // namespace nenenib::application
