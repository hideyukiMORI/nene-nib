#pragma once

#include <d2d1.h>
#include <dwrite.h>

namespace nenenib::ui::win32
{
// イージーモーションの印 1 つを描く値（ADR 0163 の決定 6 と追補 #1800・案 D）。badge は札の角丸の
// 矩形（motion_label_badge・整数の物理画素）、written は印の字の layout（札の大きさで中央に置く・
// 印の専用の太字の書式から作る・共有の本文の layout は変えない）、ground は札の地（text）、ink は
// 字と札のまわりの物理 1 画素の縁の色（background・本文の逆・トークンだけ）。
struct MotionLabelPaint
{
    D2D1_ROUNDED_RECT badge;
    IDWriteTextLayout *written;
    D2D1_COLOR_F ground;
    D2D1_COLOR_F ink;
};
} // namespace nenenib::ui::win32
