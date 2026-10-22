#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// PHP の 1 行の字句器（#860・ADR 0156 の追補）。入口は lex_line と lex_state で、
// ここを直接呼ばない。分類は Issue #860 の表（Vim の php.vim と PHP Manual の Strings /
// Heredoc に寄せる）で、キーワードと定数は ASCII の大小を問わない。既定の plain は
// `?>` の外（HTML・区間なし・`<?php` か `<?=` で中に入る）で、PHP の中は plain の
// delimiter `php`。中で行をまたぐのは `/* */`（block_comment）、`'…'` `"…"` `` `…` ``
// （raw_string の delimiter は引用符）、ヒアドキュメントとナウドキュメント
// （raw_string の delimiter は閉じの識別子）。
void lex_php(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
