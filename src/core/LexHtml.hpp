#pragma once

#include "Language.hpp"
#include "LexCursor.hpp"
#include "LexState.hpp"

namespace nenenib::core
{
// HTML と XML の 1 行の字句器（#838・ADR 0156 の追補）。入口は lex_line と lex_state で、ここを
// 直接呼ばない。分類は Vim の html.vim / xml.vim に寄せる: タグの頭 `<name` `</name` と閉じの印
// `>` `/>` は keyword・属性名は type・属性の値（引用符の中と引用符の無い値）は string・
// `<!DOCTYPE …>` などの宣言は keyword・実体参照 `&amp;` `&#123;` は constant・テキストは区間なし。
// コメント `<!-- … -->` は block_comment で行をまたぐ。language が xml のときだけ、CDATA
// `<![CDATA[ … ]]>` を string として raw_string（delimiter は `]]>`）で行をまたぎ、`<?xml … ?>` を
// タグと同じに読む。html のときだけ、`<script>` `<style>` の中身は分類せず（埋め込みは後続）、
// 閉じタグまで raw_string（delimiter は `</script` か `</style`）で行をまたぐ。行をまたぐタグの
// 属性は後続。
void lex_html(LexCursor &cursor, const LexState &in, Language language);
} // namespace nenenib::core
