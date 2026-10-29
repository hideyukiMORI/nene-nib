#pragma once

#include "GutterLabel.hpp"
#include "LineNumber.hpp"

namespace nenenib::core
{
// 行番号の欄の文字の 1 本（#1041・ADR 0008 の追補）。relative が偽なら絶対の番号を右寄せ。
// 真なら Vim の `number` と `relativenumber` の両方の形で、caret の行は絶対の番号を左寄せ、
// ほかの行はキャレットの行からの距離を右寄せ。欄の幅の規則は変えない（距離は行数より小さい）。
[[nodiscard]] GutterLabel gutter_label(LineNumber line, LineNumber caret, bool relative);
} // namespace nenenib::core
