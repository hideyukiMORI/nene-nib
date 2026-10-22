#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// Lua の 1 行の字句器（#860・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを直接
// 呼ばない。分類は Issue #860 の表（Vim の lua.vim と Lua 5.4 Reference Manual §3.1 に寄せる）で、
// ASCII の識別子だけを分類する。行をまたぐのは長いコメント `--[=*[ … ]=*]`（block_comment）と
// 長い文字列 `[=*[ … ]=*]`（raw_string）で、どちらも delimiter に開きの `=` の列を持つ。
void lex_lua(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
