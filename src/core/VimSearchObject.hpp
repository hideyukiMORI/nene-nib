#pragma once

#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimPattern.hpp"
#include "VimSearchNoticeKind.hpp"
#include "VimSearchObjectRequest.hpp"

#include <expected>

namespace nenenib::core
{
// `gn` `gN` が選ぶ当たり（Issue #1617・Vim 9.1 の search.c の current_search）。答えは VISUAL が
// 置く選択（anchor は Vim の VIsual・caret はカーソル）。NORMAL・オペレータ待ち・VISUAL の 3 つが
// 同じ 1 本を呼ぶ。探すのは vim_find_match の 1 本（ADR 0032 の決定 3・4）だけで、
// 当たりの終わりは vim_line_matches と同じ行の走査から引く。
//
// 規則は current_search を写したもの: 前向きは「1 つ手前の当たり（終わりがキャレットより前）を
// 折り返さずに後ろ向きに探し、そこから前向きに回数ぶん探す」ので、キャレットが当たりの中（最後の
// 文字の上を含む）なら今の当たり、そうでなければ次の当たり。後ろ向きは向きを逆にした同じ形。
// VISUAL では探し始めを 1 文字先（`gN` は手前）へずらし、anchor を動かさない。パターンが本文の
// どこにも当たらないか、2 回目の探索が見つからなければ pattern_not_found（Vim は報せを出さずに
// ビープする）。
[[nodiscard]] std::expected<Selection, VimSearchNoticeKind>
vim_search_object(const TextBuffer &text, const VimPattern &pattern,
                  const VimSearchObjectRequest &request);
} // namespace nenenib::core
