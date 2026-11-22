#pragma once

#include "CaretQuad.hpp"

#include <d2d1.h>

namespace nenenib::ui::win32
{
// 動いているキャレットの 1 枚の描き方（ADR 0192 の決定 4）。shown は描く四隅、from と to は勾配の
// 向き（出発と行き先）、head は頭（キャレット）の色、tail は尾の先の色、glow はにじみの広がり
// （物理画素）。公開 aggregate（ADR 0007）。
struct CaretMotionPaint
{
    core::CaretQuad shown;
    core::CaretQuad from;
    core::CaretQuad to;
    D2D1_COLOR_F head;
    D2D1_COLOR_F tail;
    float glow;
};
} // namespace nenenib::ui::win32
