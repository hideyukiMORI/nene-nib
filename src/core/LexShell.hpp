#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// shell（bash / sh / zsh 共通）の 1 行の字句器（#862・ADR 0156 の追補）。入口は lex_line と
// lex_state で、ここを直接呼ばない。分類は Issue #862 の表（Vim の sh.vim の bash の既定と
// Bash Reference Manual の Quoting と Here Documents に寄せる）。行をまたぐのは `'…'`
// （raw_string の delimiter `'`）・`"…"`（`"`）・`$'…'`（`$'`）とヒアドキュメント `<<EOF`
// `<<-EOF` `<<'EOF'` `<<"EOF"`（delimiter は頭の印と閉じの語・印は `<`、`<<-` は行頭のタブを
// 許す `-`・語は 15 バイトまで）。1 行に 2 つ以上のヒアドキュメントがあれば最初の 1 つだけ。
void lex_shell(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
