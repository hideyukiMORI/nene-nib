#pragma once

#include "MotionLabel.hpp"
#include "MotionLabelPaint.hpp"

#include <d2d1.h>
#include <dwrite.h>

#include <cstdint>

namespace nenenib::ui::win32
{
// 札の角丸の矩形（ADR 0163 の追補 #1800・案 D'＝行の中の上寄せ）。札は跳び先の自分の行の中に
// 置き、行の上へ出さない。target は跳び先の当たり矩形（行の layout の HitTestTextRange・字の無い
// 跳び先は位置と行の高さだけ・折り返しと横の送りと DPI を含んだ位置）、label は印（字の数と
// 跳び先の升の数）、written は札の字の layout の大きさ（印の書式で測った幅と行の高さ）。
// 上端は行の上端 + 物理 1 画素（縁の 1 画素が行の上端に載る）、左端は升の左端。高さは字の行の
// 高さ（上下の余白 0）で、行の高さの 6 割を超えるなら詰める。幅は字の幅 + 左右 2 DIP までの余白で、
// 升 × 印の字の数（全角の跳び先は升 1 つが半分の幅・2 文字の札は全角 1 字ぶんに収まる）を超える
// なら余白を物理 1 画素まで詰める（字の画を札の縁に付けない）。半角の跳び先の 2
// 文字の札は次の升へ出る。座標と半径は整数の物理画素で、 半径は 2 DIP を DPI で丸めて 2〜3
// 画素に収める。
[[nodiscard]] D2D1_ROUNDED_RECT motion_label_badge(const DWRITE_HIT_TEST_METRICS &target,
                                                   const core::MotionLabel &label,
                                                   D2D1_SIZE_F written, std::uint32_t dpi) noexcept;

// 札の字の大きさ（DIP）。本文の 6 割で、下限は 10 DIP（それ未満は l i 1 が潰れる・字体の大きさは
// DPI で比例する）。
[[nodiscard]] float motion_label_text_dips(float body_dips) noexcept;

// 印を 1 つ描く。札のまわりに ink の物理 1 画素の縁（下の字や隣の札と分ける・整数の画素で
// 塗る）、不透明の札を ground で塗り、札に切り抜いて字を ink で描く（字の layout は札の大きさで
// 中央に寄せてある）。製品の renderer と tests/ui が同じ 1 本を通る。Target は
// ID2D1RenderTarget（製品は device context）。
template <typename Target>
void paint_motion_label(Target &target, ID2D1SolidColorBrush *brush, const MotionLabelPaint &paint)
{
    const auto &shape = paint.badge.rect;
    brush->SetColor(paint.ink);
    target.FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(shape.left - 1.0F, shape.top - 1.0F, shape.right + 1.0F,
                                      shape.bottom + 1.0F),
                          paint.badge.radiusX + 1.0F, paint.badge.radiusY + 1.0F),
        brush);
    brush->SetColor(paint.ground);
    target.FillRoundedRectangle(paint.badge, brush);
    if (paint.written == nullptr)
    {
        return;
    }
    brush->SetColor(paint.ink);
    target.PushAxisAlignedClip(shape, D2D1_ANTIALIAS_MODE_ALIASED);
    target.DrawTextLayout(D2D1::Point2F(shape.left, shape.top), paint.written, brush);
    target.PopAxisAlignedClip();
}
} // namespace nenenib::ui::win32
