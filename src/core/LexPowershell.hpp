#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// PowerShell の 1 行の字句器（#862・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを
// 直接呼ばない。分類は Issue #862 の表（Vim の ps1.vim と about_Parsing / about_Quoting_Rules に
// 寄せる）で、キーワードと比較の演算子は ASCII の大小を問わない。行をまたぐのは `<# #>`
// （block_comment）・`'…'`（raw_string の delimiter `'`）・`"…"`（`"`）・ヒア文字列 `@'` `@"`
// （行末の開きだけ・delimiter `@'` `@"`・閉じは行頭の `'@` `"@` だけ）。
void lex_powershell(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
