#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// YAML の 1 行の字句器（#838・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを直接
// 呼ばない。分類は Vim の yaml.vim に寄せる: `#` のコメント（行頭か空白の後）・行頭のキー（字下げと
// リストの記号の後の裸のスカラーで、後ろに `: ` か行末の `:` が続く）と flow の中のキーは type・
// リストの記号 `- ` と flow の記号 `[ ] { } ,` は operators・`'…'` `"…"` は string（閉じない行は
// 行末まで string・複数行の引用は後続）・スカラー全体が `true false null ~ yes no`（小文字・頭が
// 大文字・全部大文字）なら constant・数（YAML 1.2 の core schema）なら number・アンカーとエイリアス
// `&name` `*name` と文書の区切り `---` `...` は keyword。ブロックスカラーの印 `|` `>`（後ろの
// `+ -` と数字も）は operators で、次の行から raw_string（delimiter は印の行の字下げの深さの 2 桁
// の 10 進・最大 99）に入る。その中の行は、空行と字下げがそれより深い行が全体で string で状態を
// 保ち、同じかより浅い行で plain に戻ってその行を普通に読む。タグ `!!str` は区間なし（後続）。
void lex_yaml(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
