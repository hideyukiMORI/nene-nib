#pragma once

#include "Language.hpp"
#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// JavaScript と TypeScript の 1 行の字句器（#833・ADR 0156 の追補）。
// 入口は lex_line と lex_state で、ここを直接呼ばない。
// 分類は Vim の javascript.vim / typescript.vim に寄せ、ASCII の識別子（`$` を含む）だけを
// 分類する。language が typescript のときだけ TS の予約語を keyword に、型の表を type にする。
// `/* … */` は block_comment、テンプレート `` `…` `` は raw_string で delimiter に `` ` `` を
// 持って行をまたぐ。`/` は直前の字句（LexCursor::precedent）で正規表現リテラルか除算かを決める。
void lex_javascript(LexCursor &cursor, const LexState &in, Language language);
} // namespace nenenib::core
