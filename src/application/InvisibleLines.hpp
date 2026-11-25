#pragma once

#include "EditorState.hpp"
#include "LineView.hpp"

#include <vector>

namespace nenenib::application
{
// 不可視文字の欄（ADR 0193 の決定 6・10・14・15・#1977）。
//
// mark_invisibles は枠の見えている行（visible_lines の 1 本を通る両枠）に LineInvisibles を置く。
// `features=list` がオフなら何もしない（欄を作らない・真偽の読み 1 つ）。区間は見えている行の本文を
// 1 回歩いて作り（core::invisible_spans）、改行の形は行の終わりの 1〜2 バイトを Vim のモードが見る
// 本文（vim_text）から読む（Vim のモードで隠す最後の改行は印なし・frame の hidden_line_break と同じ
// 条件）。巨大な行の窓・IME の差し込み行には欄を置かない（決定 14・15）。
void mark_invisibles(const EditorState &state, std::vector<LineView> &lines);
} // namespace nenenib::application
