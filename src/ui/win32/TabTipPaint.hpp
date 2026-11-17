#pragma once

#include <d2d1.h>
#include <dwrite.h>

namespace nenenib::ui::win32
{
// タブの札 1 つを描く値（#1789・ADR 0056 の #1789 の追補）。box は札の矩形（core::tab_tip_rect）、
// written は文の layout（TabTipLayout が真ん中で切った 1 行）、origin は文の左上（box の左上 +
// 余白）、radius と stroke は角丸と縁の太さ（物理画素）。色は
// ground（panel）・edge（panel_border）・ ink（text）のトークンだけ（ADR 0008 の決定 8）。
struct TabTipPaint
{
    D2D1_RECT_F box;
    IDWriteTextLayout *written;
    D2D1_POINT_2F origin;
    float radius;
    float stroke;
    D2D1_COLOR_F ground;
    D2D1_COLOR_F edge;
    D2D1_COLOR_F ink;
};
} // namespace nenenib::ui::win32
