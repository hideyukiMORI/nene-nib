#pragma once

#include "LineView.hpp"

#include <d2d1.h>

#include <algorithm>
#include <span>

namespace nenenib::ui::win32
{
// イージーモーションの印が出ている（見えている行のどれかの LineView.labels が空でない）。
// 出ている間は操作中の枠のブロックのキャレットを塗らずに枠だけで描く（塗ったブロックの下の字が
// 札の印に見えないように・ADR 0163 の追補 #1800）。
[[nodiscard]] inline bool motion_labels_shown(std::span<const application::LineView> lines)
{
    return std::ranges::any_of(lines, [](const application::LineView &line)
                               { return !line.labels.empty(); });
}

// ブロックのキャレットの升 1 つを stroke の太さの枠で囲み、中は塗らない（枠は升の内側に収める）。
// 色は呼び手が brush に置く。もう 1 つの枠の印（muted）と印が出ている間（accent）の 2 つが通り、
// 製品の renderer と tests/ui が同じ 1 本を通る。Target は ID2D1RenderTarget（製品は device
// context）。
template <typename Target>
void outline_caret_block(Target &target, ID2D1SolidColorBrush *brush, const D2D1_RECT_F &block,
                         float stroke)
{
    const float inset = stroke / 2.0F;
    target.DrawRectangle(D2D1::RectF(block.left + inset, block.top + inset, block.right - inset,
                                     block.bottom - inset),
                         brush, stroke);
}
} // namespace nenenib::ui::win32
