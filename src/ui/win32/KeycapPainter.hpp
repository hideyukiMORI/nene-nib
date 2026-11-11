#pragma once

#include "KeycapStyle.hpp"
#include "LayoutRect.hpp"

#include <d2d1.h>
#include <dwrite.h>
#include <tuple>

namespace nenenib::ui::win32
{
// 鍵の枠を描く 1 本（本文の案内・一覧の鍵・起動画面の鍵・ADR 0172 の決定 5）。文字の左右に
// style.padding・高さ style.height・角丸 style.radius（どれも DIP で倍率を掛ける）の枠を、右端を
// 欄 note の右端に揃えて行の縦の中央に置き、面 face・縁 border・字 text で描く。幅は shown の
// metrics（左寄せの書式の layout・#258）で、欄より広ければ欄の幅で切る。欄が空か layout が無ければ
// 何もしない。canvas は描画先・brush・DIP から物理画素への倍率（DPI / 96）の組で、描画先は製品では
// Direct2D の描画先、tests/ui は WIC の bitmap の描画先。
using KeycapCanvas = std::tuple<ID2D1RenderTarget &, ID2D1SolidColorBrush &, float>;
void paint_keycap(const KeycapCanvas &canvas, IDWriteTextLayout *shown,
                  const core::LayoutRect &note, const KeycapStyle &style);
} // namespace nenenib::ui::win32
