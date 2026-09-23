#pragma once

#include "OffsetRange.hpp"
#include "TextBuffer.hpp"
#include "VimMatchRequest.hpp"
#include "VimPattern.hpp"
#include "VimSearchHit.hpp"
#include "VimSearchNoticeKind.hpp"

#include <expected>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 1 行の中の一致を 0 桁目から順に数えた列（ADR 0037 の決定 3）。走査の規則は vim_find_match と
// 同じ 1 本で、重なる一致は飛ばし（"ababa" の /aba は 1 つ・"aaaa" の /aa は 2 つ）、長さ 0 の
// 一致も 1 つと数えて 1 文字進む（"abc" の /a* は 3 つ・固定 Vim 9.1 の searchcount() で実測）。
// 位置は行頭からの相対バイトで、行をまたがない。塗る面を持たない長さ 0 の一致を捨てるかは
// 呼ぶ側が決める。
[[nodiscard]] std::vector<OffsetRange> vim_line_matches(std::string_view line,
                                                        const VimPattern &pattern);

// 次の一致を求める 1 本の経路（ADR 0032 の決定 3・4 の追記・ADR 0041 の決定 1）。純関数で、
// 本文・履歴・状態を持たない。確定の検索の鍵（`/` `?` の Enter・`n` `N` `*` `#`）と
// incsearch の preview の両方がこれを呼ぶ。
//
// 1 回ぶんは、前向きは request.from の次の文字以降で最初の一致、後ろ向きは request.from
// より前の最後の一致。 行ごとに 0 桁目から数え直し、必要な桁に足りない一致は終端（長さ 0 なら 1
// 文字先）まで 飛ばして数え直し、位置が行の長さに達したらその行を打ち切る（"aaaa" の /aa は 3
// 桁目、 "abc" の /.* は動かない）。一致は行をまたがない。wrapscan は既定どおり有効で、
// 本文の端を越えて起点の行まで戻る。
//
// 回数は着いた位置からもう一度探して数える（`3/x` と `/x` ＋ `3n` が同じ・実測）。wrapped は
// どれか 1 回でも折り返したら立つ。1 回でも見つからなければ pattern_not_found で、
// 呼ぶ側が E486 を出して命令ごと取り消す（実測）。
[[nodiscard]] std::expected<VimSearchHit, VimSearchNoticeKind>
vim_find_match(const TextBuffer &text, const VimPattern &pattern, const VimMatchRequest &request);
} // namespace nenenib::core
