#pragma once

#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// Markdown の 1 行の字句器（#833・ADR 0156 の追補）。入口は lex_line と lex_state で、
// ここを直接呼ばない。分類は Vim の markdown.vim に寄せる:
// 見出しの行は keyword・強調は constant・行内コードとコードブロックは string・
// リンクと画像は function・引用と水平線の行は comment・箇条書きの記号は operators・
// HTML のタグは keyword。フェンス（行頭の空白 3 つまでの ``` か ~~~・3〜16 文字）は
// raw_string の状態で delimiter にフェンスの文字列を持って行をまたぎ、
// 同じ文字で同じ長さ以上のフェンスだけの行で閉じる（CommonMark）。
// フェンスの中の言語の埋め込みは後続。
void lex_markdown(LexCursor &cursor, const LexState &in);
} // namespace nenenib::core
