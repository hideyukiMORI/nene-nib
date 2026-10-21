#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// TOML の 1 行の字句器（#838・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを直接
// 呼ばない。分類は Vim の toml.vim に寄せる: `#` のコメント・行頭の `[table]` `[[array]]` は
// keyword（中身がキーの形で、閉じの後ろが空白かコメントだけのとき）・キー（行頭か、インライン
// テーブルの `{` `,` の後ろの、裸のキーと引用符のキーを `.` でつないだもので、後ろに `=` が続く）は
// type・`"…"` `'…'` は string（閉じない行は error）・`"""` `'''` は Python と同じく raw_string で
// delimiter にその 3 文字を持って行をまたぐ・数と日時（`1_000` `0xff` `+inf` `1979-05-27T07:32:00Z`
// を検証せずに）は number・`true false` は constant・`= , [ ] { } .` は operators。
void lex_toml(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
