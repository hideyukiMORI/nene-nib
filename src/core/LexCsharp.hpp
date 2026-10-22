#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// C# の 1 行の字句器（#857・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを直接
// 呼ばない。分類は Issue #857 の表（Vim の cs.vim と C# の言語仕様に寄せる）で、ASCII の識別子
// だけを分類する。行をまたぐのは `/* */`（block_comment）と、raw_string の状態で delimiter に
// 区切りを持つ逐語的文字列（`@"…"` は `@`・`$@"…"` `@$"…"` は `$@`）と生の文字列（`"""…"""` は
// 開きの引用符の列そのもの・補間の `$"""…"""` も同じ）。補間の `{…}` の中は分類しない。
void lex_csharp(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
