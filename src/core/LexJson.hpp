#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// JSON の 1 行の字句器（#830・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを直接
// 呼ばない。JSON の字句は行をまたがないので、どの状態で入っても行頭から読み、出る状態は plain。
// 分類は Vim の json.vim に寄せる: 文字列はキーも値も string（閉じなければ行末まで error）・数字・
// true false null は constant・`{ } [ ] : ,` は operators・裸の識別子は区間なし。
void lex_json(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
