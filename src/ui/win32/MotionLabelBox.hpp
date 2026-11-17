#pragma once

#include "MotionLabel.hpp"
#include "MotionLabelPaint.hpp"

#include <d2d1.h>
#include <dwrite.h>

namespace nenenib::ui::win32
{
// 印の地の矩形（ADR 0163 の決定 6）。target は跳び先の文字の当たり矩形（行の layout の
// HitTestTextRange・折り返しと横の送りと DPI を含んだ位置）で、升は target の幅 ÷ cells。幅は
// 升 × max(cells, 印の字の数)（半角の跳び先に 2 文字の印は次の升へはみ出す）、高さは target
// と同じ。 角は丸めない。
[[nodiscard]] D2D1_RECT_F motion_label_box(const DWRITE_HIT_TEST_METRICS &target,
                                           const core::MotionLabel &label) noexcept;

// 印を 1 つ描く（地を塗り、地に切り抜いて字を左に寄せて描く）。製品の renderer と tests/ui が
// 同じ 1 本を通る。Target は ID2D1RenderTarget（製品は device context）。
template <typename Target>
void paint_motion_label(Target &target, ID2D1SolidColorBrush *brush, const MotionLabelPaint &paint)
{
    brush->SetColor(paint.ground);
    target.FillRectangle(paint.box, brush);
    if (paint.written == nullptr)
    {
        return;
    }
    brush->SetColor(paint.ink);
    target.PushAxisAlignedClip(paint.box, D2D1_ANTIALIAS_MODE_ALIASED);
    target.DrawTextLayout(D2D1::Point2F(paint.box.left, paint.box.top), paint.written, brush);
    target.PopAxisAlignedClip();
}
} // namespace nenenib::ui::win32
