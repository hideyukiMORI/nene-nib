#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// C と C++ の 1 行の字句器（#815・ADR 0156 の決定 5・6）。入口は lex_line と lex_state
// で、ここを直接 呼ばない。cursor の行を in の状態から読み、区間と出る状態を cursor に残す。分類は
// Vim の c.vim / cpp.vim に寄せ、ASCII の識別子だけを分類する。
void lex_cpp(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
