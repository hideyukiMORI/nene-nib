#pragma once

#include "PreviewAnchor.hpp"
#include "PreviewLine.hpp"
#include "TextBuffer.hpp"

#include <optional>
#include <vector>

namespace nenenib::application
{
// 手元の本文 text から窓を作る（ADR 0189 の決定 4・開いているタブの文書）。anchor があればその行の
// 前 finder_preview_context_lines 行から、無ければ先頭から、最大 finder_preview_line_limit 行。
// 1 行は finder_preview_line_bytes バイトでコードポイントを割らずに切り（行の全体を写さない）、
// Tab のほかの制御文字は 1 バイトの空白に置き換える。anchor が本文の外なら先頭から（印は
// 付かない）。
[[nodiscard]] std::vector<PreviewLine> preview_lines(const core::TextBuffer &text,
                                                     const std::optional<PreviewAnchor> &anchor);
} // namespace nenenib::application
