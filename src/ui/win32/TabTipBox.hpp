#pragma once

#include "TabTipPaint.hpp"

#include <d2d1.h>

namespace nenenib::ui::win32
{
// タブの札を描く（#1789）。地を角丸で塗り、縁を札の内側に 1 本引き、札に切り抜いて文を描く。影と
// 動きは付けない。製品の renderer と tests/ui が同じ 1 本を通る。Target は ID2D1RenderTarget
// （製品は device context）。
template <typename Target>
void paint_tab_tip(Target &target, ID2D1SolidColorBrush *brush, const TabTipPaint &paint)
{
    const float half = paint.stroke / 2.0F;
    brush->SetColor(paint.ground);
    target.FillRoundedRectangle(D2D1::RoundedRect(paint.box, paint.radius, paint.radius), brush);
    brush->SetColor(paint.edge);
    const D2D1_RECT_F inner = D2D1::RectF(paint.box.left + half, paint.box.top + half,
                                          paint.box.right - half, paint.box.bottom - half);
    target.DrawRoundedRectangle(D2D1::RoundedRect(inner, paint.radius, paint.radius), brush,
                                paint.stroke);
    if (paint.written == nullptr)
    {
        return;
    }
    brush->SetColor(paint.ink);
    target.PushAxisAlignedClip(paint.box, D2D1_ANTIALIAS_MODE_ALIASED);
    target.DrawTextLayout(paint.origin, paint.written, brush);
    target.PopAxisAlignedClip();
}
} // namespace nenenib::ui::win32
