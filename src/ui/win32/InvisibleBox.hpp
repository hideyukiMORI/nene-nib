#pragma once

#include "InvisibleKind.hpp"

#include <d2d1.h>

namespace nenenib::ui::win32
{
// 印を描く 1 字の升（ADR 0193 の決定 10・#1977）。box は字の左右の端と画面行の上下（layout の座標か
// 描く先の座標・呼び手が決める）、kind は印の形と色。
struct InvisibleBox
{
    D2D1_RECT_F box;
    core::InvisibleKind kind;
};
} // namespace nenenib::ui::win32
