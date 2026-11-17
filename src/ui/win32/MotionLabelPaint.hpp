#pragma once

#include <d2d1.h>
#include <dwrite.h>

namespace nenenib::ui::win32
{
// イージーモーションの印 1 つを描く値（ADR 0163 の決定 6）。box は地の矩形（motion_label_box）、
// written は印の字の layout（行の layout の書式から作った別の layout・共有の本文の layout は
// 変えない）、ground と ink は地と字の色（accent と on_accent・トークンだけ）。
struct MotionLabelPaint
{
    D2D1_RECT_F box;
    IDWriteTextLayout *written;
    D2D1_COLOR_F ground;
    D2D1_COLOR_F ink;
};
} // namespace nenenib::ui::win32
