#pragma once

#include "EditorState.hpp"
#include "GridDiagnosticRange.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"

#include <vector>

namespace nenenib::application
{
// ADR 0161 の決定 4: 見えている行の診断の下線の区間を、構文の色（SyntaxLines）と同じ道で frame が
// 見えている行ごとに作る。文書に診断が無ければ何もしない（off の文書は診断を持たない）。

// 見えている 1 行の下線の区間を view.diagnostics に置く。区間は core の diagnostic_spans の 1 本で
// 作る。巨大な行の窓は区間を付けない（色と同じ限界）。IME の差し込み行は色と同じく差し込みの位置で
// 割り、後ろを差し込んだ code point の数だけずらす。
void diagnosed_line(const EditorState &state, LineView &view);

// 格子の 1 行。行の区間を本文の位置へ写して diagnostics
// に置く。巨大な行は色と同じく区間を付けない。
void grid_diagnosed_line(const EditorState &state, core::LineNumber line,
                         std::vector<GridDiagnosticRange> &diagnostics);
} // namespace nenenib::application
