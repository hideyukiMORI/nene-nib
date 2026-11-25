#pragma once

#include "InvisibleMark.hpp"
#include "LineView.hpp"

#include <dwrite.h>
#include <optional>
#include <vector>

namespace nenenib::ui::win32
{
// 行の不可視文字の欄を、描いた layout の UTF-16 の位置へ写す（ADR 0193 の決定 10・#1977）。本文の桁
// → 描画の位置（core::display_position・`^A` などの置き換えの後ろもずれない）→ UTF-16
// の位置。区間の 1 字ごとに 1 つ・前から順。置き換えられた字（描画で 2
// 字以上）には印を付けない。欄が無ければ空。
[[nodiscard]] std::vector<InvisibleMark> invisible_marks_of(const application::LineView &line);

// 行の終わり（描画の文字列の末尾）の UTF-16 の位置。改行の印はこの位置の x（折り返した行は最後の
// 画面行）に置く。
[[nodiscard]] UINT32 line_end_unit(const application::LineView &line);

// 改行の印の左端。行の終わり x が本文の欄の右端 right
// より右なら（行が右の外へ続く）値なしで描かない。 印の幅 width
// が右端からはみ出すなら、右端に収まるよう左へ寄せる（幅いっぱいの行で印が切り抜きの
// 外に消えない・#2047 のレビュー K4）。
[[nodiscard]] std::optional<float> line_break_left(float x, float width, float right) noexcept;
} // namespace nenenib::ui::win32
