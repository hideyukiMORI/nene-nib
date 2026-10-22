#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// SQL の 1 行の字句器（#860・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを直接
// 呼ばない。分類は Issue #860 の表（ISO SQL と主要な方言の共通部分・Vim の sql.vim に寄せる）で、
// キーワードは ASCII の大小を問わない。行をまたぐのは `/* */`（block_comment）と `'…'`
// （raw_string の delimiter `'`・`''` は閉じない）。
void lex_sql(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
