#pragma once

#include "EditorState.hpp"
#include "LineView.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::application
{
// インデントガイドの欄（ADR 0193 の決定 6・7・9・14・15・#1975）。
//
// mark_indent_guides は枠の見えている行（visible_lines の 1 本を通る両枠）に IndentGuides を置く。
// `features=indentguides` がオフなら何もしない（欄を作らない・真偽の読み 1 つ）。字下げは見えている
// 行の本文から数え、行の外を見るのは見えている範囲の端の空の行の続きだけ（上下それぞれ screen 行
// まで・届かなければ届いた側の値）。段の幅は文書の推し当て（Document::indent_unit）か設定の
// indent_guide_width。巨大な行の窓・IME の差し込み行には欄を置かない（字下げの数えには使う）。
void mark_indent_guides(const EditorState &state, std::size_t screen, std::vector<LineView> &lines);

// 強める段（決定 9）。操作中の枠の行だけに写す（finish_frame が呼ぶ）。設定の
// indent_guide_highlight_current
// がオフ・キャレットの行が見えていない・キャレットの行に欄が無ければ何もしない。区間は見えている行の
// 中で打ち切る（core の active_indent_run）。
void mark_active_indent_guide(const EditorState &state, std::vector<LineView> &lines);
} // namespace nenenib::application
