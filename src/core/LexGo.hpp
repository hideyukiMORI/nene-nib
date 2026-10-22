#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// Go の 1 行の字句器（#857・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを直接
// 呼ばない。分類は Issue #857 の表（Vim の go.vim と The Go Programming Language Specification の
// Lexical elements に寄せる）で、ASCII の識別子だけを分類する。行をまたぐのは `/* */`
// （block_comment）と生の文字列 `` `…` ``（raw_string の delimiter `` ` ``）。
void lex_go(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
