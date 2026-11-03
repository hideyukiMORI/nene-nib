#pragma once

#include "Diagnostic.hpp"
#include "DiagnosticSpan.hpp"
#include "Edit.hpp"
#include "Offset.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 編集で診断の位置をずらす唯一の 1 本（ADR 0161 の決定 3）。端の写しは枠の位置と同じ
// offset_after_edit（EditAdjust・ADR 0148 の決定 9）を使い、規則を別に持たない。
// 編集の範囲に掛かる診断（消した範囲 [at, at + removed) と重なる・純粋な挿入なら区間の内側に
// 入る）は消す。編集より前の診断はそのまま、後ろの診断は削除と挿入の差だけずらす（ちょうど at から
// 始まる診断は挿入の後ろへ）。残った診断の並びは保つ。diagnostics は編集の前の本文の上の位置。
[[nodiscard]] std::vector<Diagnostic> diagnostics_adjusted(std::vector<Diagnostic> diagnostics,
                                                           const Edit &edit);

// 見えている 1 行の下線の区間（ADR 0161 の決定 4）。line は行の内容（改行を含まない）、line_start
// は その行頭の本文のバイト位置。診断の区間を行の内容へ切り、桁（code point の序数・行頭が 1）の
// 昇順で重ならない列にする。重なる所は重い方だけ（error > warning > information > hint）で、隣り
// 合う同じ重さはつなぐ。空の診断（begin == end）は行の内容の中か行末にあれば、その桁から 1 桁
// ぶんの区間にする（行末では内容の外の 1 桁）。改行のバイトだけに掛かる診断も行末の 1 桁にする
// （内容と改行をまたぐ診断は内容の上だけ）。
[[nodiscard]] std::vector<DiagnosticSpan> diagnostic_spans(std::string_view line, Offset line_start,
                                                           std::span<const Diagnostic> diagnostics);
} // namespace nenenib::core
