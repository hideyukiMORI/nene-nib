#pragma once

#include "Language.hpp"
#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// Ini（dosini）と Godot の設定（gdresource）の 1 行の字句器（#855・ADR 0156 の追補）。
// 入口は lex_line と lex_state で、ここを直接呼ばない。2 つは同じ「見出し・キー・`=`・値」
// の行で、language の印で違う所だけを分ける（HTML / XML が lex_html を共用するのと同じ形）。
// dosini は Vim の dosini.vim に寄せる: 行頭（空白の後）の `;` `#` はコメント・
// `[` から `]` までの見出しは keyword・`=` の前のキーは type・`=` は operators・
// 値は区間なしで、数だけの値は number・値の中の `"…"` は string。行をまたがない。
// gdresource は Vim の gdresource.vim に寄せる: `;` から行末はコメント・
// 見出し `[name` と閉じの `]` は keyword・見出しの属性名と行頭のキー
// （`config/name` `0:0/0` のような `/` `:` `.` `-` を含む名前）は type・`=` は operators・
// `"…"` `&"…"` `^"…"` は string で、`"` を区切りの raw_string として行をまたぐ
// （`\` は次の 1 バイトを飛ばす）・数は number・`true false null` は constant・
// `(` の直前の名前（`Vector2i(` `ExtResource(` `PackedStringArray(`）は type。
void lex_ini(LexCursor &cursor, const LexState &in, Language language);
} // namespace nenenib::core
