#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// CSS の 1 行の字句器（#838・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを直接
// 呼ばない。分類は Vim の css.vim に寄せる: セレクタと値の名前は区間なし・`{ } : ; ,` は
// operators・プロパティ名は type・数字と単位（`12px` `1.5em` `50%`）は number・色 `#fff`
// （16 進の 3 / 4 / 6 / 8 桁）は constant・文字列は string（閉じない行は error）・at-rule
// （`@media` など）と `!important` は keyword。`/* … */` は block_comment で行をまたぐ。
// プロパティ名は「行頭か `{` `;` の後の名前で、後ろに `:` が続き、その先が `;` `}` 行末より前に
// `{` を持たない」名前（`{` の深さは行をまたいで数えない・`a:hover {` はセレクタ）。
void lex_css(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
