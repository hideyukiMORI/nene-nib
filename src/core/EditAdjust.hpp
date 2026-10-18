#pragma once

#include "Edit.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"

namespace nenenib::core
{
// もう 1 つの枠の位置を、操作中の枠の編集 edit の後の本文の位置へ写す（ADR 0148 の決定 9）。
// 編集より前は不変・削除の範囲の後ろは削除と挿入の差だけずらす・削除の範囲の中は at へ畳む・
// ちょうど at は挿入の後ろへずらす。undo は inserted と removed を入れ替えた編集を当てる。
[[nodiscard]] Offset offset_after_edit(Offset offset, const Edit &edit) noexcept;

// 行番号 line を、行 edit_line で始まった編集 edit の後の行番号へ写す。編集の行までは不変・
// 消えた行の後ろは改行の増減だけずらす・消えた行の中は編集の行へ畳む。改行は LF の数で数える
// （CRLF も LF を 1 つ持つ）。
[[nodiscard]] LineNumber line_after_edit(LineNumber line, LineNumber edit_line,
                                         const Edit &edit) noexcept;
} // namespace nenenib::core
