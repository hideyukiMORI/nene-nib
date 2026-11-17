#pragma once

#include "Language.hpp"
#include "PreviewAnchor.hpp"
#include "PreviewLine.hpp"
#include "TextBuffer.hpp"

#include <optional>
#include <vector>

namespace nenenib::application
{
// 本文 text から窓を作る 1 本（ADR 0189 の決定 4・5）。開いているタブの手元の本文と、worker が
// 読んで復号した本文（段 P2 の adapter）が同じ形の窓をこれで作る。anchor があればその行の前
// finder_preview_context_lines 行から、無ければ先頭から、最大 finder_preview_line_limit 行。
// 改行で終わる本文の最後の空の行は窓に入れない。1 行は finder_preview_line_bytes バイトで
// コードポイントを割らずに切り（行の全体を写さない）、本文と同じ core::display_line の形にする。
// 構文の色は窓の先頭の行から language の字句器（core::lex_line）を走らせる（窓が複数行のコメントの
// 途中から始まると色がずれる・限界）。anchor が本文の外なら先頭から（印は付かない）。
[[nodiscard]] std::vector<PreviewLine> preview_lines(const core::TextBuffer &text,
                                                     const std::optional<PreviewAnchor> &anchor,
                                                     core::Language language);
} // namespace nenenib::application
