#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// Rust の 1 行の字句器（#857・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを直接
// 呼ばない。分類は Issue #857 の表（Vim の rust.vim と The Rust Reference の Tokens に寄せる）で、
// ASCII の識別子だけを分類する。`/* */` は入れ子で、block_comment の delimiter に深さを 10 進で
// 持つ。文字列 `"…"` `b"…"` `c"…"` は raw_string の delimiter `"`、生の文字列 `r#"…"#` は
// delimiter に `r` と `#` の列を持って行をまたぐ。
void lex_rust(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
