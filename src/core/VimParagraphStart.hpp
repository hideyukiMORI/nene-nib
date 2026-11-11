#pragma once

#include "LineNumber.hpp"
#include "TextBuffer.hpp"

namespace nenenib::core
{
// 行が段落か節の始まりか（Vim 9.1 の textobject.c の startPS(lnum, NUL, FALSE)・Issue #1359・
// ADR 0031 の決定 9 と 10 の追補）。空の行、改ページ（form feed）で始まる行、`.` の後ろの 2 文字が
// 既定の 'paragraphs'（`IPLPPPQPP TPHPLIPpLpItpplpipbp`）か 'sections'（`SHNHH HUnhsh`）の
// nroff のマクロの名前に当たる行（Vim の inmacro・名前の空白は行の終わりか空白に当たる）。
// 行頭の `{` は数えない（cpo-{ は既定に無い）。段落の移動 `{` `}`、`ip` `ap`、文の移動 `(` `)` と
// `is` `as` の段落の端がこの 1 本を呼ぶ（ARC-001）。行は写さず、行頭の 3 バイトだけを読む。純関数。
[[nodiscard]] bool vim_paragraph_start(const TextBuffer &text, LineNumber line);
} // namespace nenenib::core
