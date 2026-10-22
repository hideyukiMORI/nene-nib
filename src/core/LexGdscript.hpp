#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// GDScript の 1 行の字句器（#855・ADR 0156 の追補）。入口は lex_line と lex_state で、
// ここを直接呼ばない。分類は Issue #855 の表（Vim の gdscript.vim に寄せる）で、
// ASCII の識別子だけを分類する。3 連引用符の文字列（`"""` `'''`）は Python と同じく
// raw_string の状態で delimiter にその 3 文字を持って行をまたぐ。
// 注釈 `@name` は行のどこでも keyword（GDScript に行列の積は無い）。
// `$` と式の始まりの `%` の後ろのノードのパス（名前と `/`・または引用符の 1 区間）は constant。
void lex_gdscript(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
