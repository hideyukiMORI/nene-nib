#pragma once

#include "Edit.hpp"
#include "Offset.hpp"

namespace nenenib::core
{
// もう 1 つの枠の位置を、操作中の枠の編集 edit の後の本文の位置へ写す（ADR 0148 の決定 9）。
// 編集より前は不変・削除の範囲の後ろは削除と挿入の差だけずらす・削除の範囲の中は at へ畳む・
// ちょうど at は挿入の後ろへずらす。undo は inserted と removed を入れ替えた編集を当てる。
// 先頭行は「その行の行頭の offset をこれで写し、編集の後の本文で行番号に戻す」で動かす（行の
// 規則を別に持たない）。
[[nodiscard]] Offset offset_after_edit(Offset offset, const Edit &edit) noexcept;
} // namespace nenenib::core
