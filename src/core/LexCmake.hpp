#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// CMake の 1 行の字句器（#862・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを直接
// 呼ばない。分類は Issue #862 の表（Vim の cmake.vim と cmake-language(7) に寄せる）で、
// コマンドと定数は ASCII の大小を問わない。行をまたぐのは括弧のコメント `#[=*[ ]=*]`
// （block_comment）と括弧の引数 `[=*[ ]=*]`（raw_string）で、どちらも delimiter に開きの `=`
// の列を持つ（Lua の長い括弧と同じ形）。`"…"` は raw_string の delimiter `"`。
void lex_cmake(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
