#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// Python の 1 行の字句器（#830・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを直接
// 呼ばない。分類は Vim の python.vim に寄せ、ASCII の識別子だけを分類する。3 連引用符の文字列
// （`"""` `'''`）は raw_string の状態で delimiter にその 3 文字を持って行をまたぐ。
void lex_python(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
