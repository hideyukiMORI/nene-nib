#pragma once

#include "OffsetRange.hpp"
#include "TextBuffer.hpp"
#include "VimMatchRequest.hpp"
#include "VimPattern.hpp"
#include "VimSearchHit.hpp"
#include "VimSearchNoticeKind.hpp"

#include <expected>
#include <optional>
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

// 行の一部を写した text の window の中の一致（#556・ADR 0140 の決定 5 の追記）。走査の規則は
// vim_line_matches と同じ 1 本で、数え始めは window の始まり、window の終わりより前に始まる一致
// だけを返し、終わりは window の終わりで切る（窓の境目で切れる・ADR 0139 の限界と同じ）。text は
// window の前後に文脈を持てて、`^` は text の始まり・`$` は text の終わりでだけ、`\<` `\>` は
// 隣の文字を見て当たる。位置は text の始まりからの相対バイト。数え始めが行頭でないので、一致が
// 連続しうる検索（例 `/aa` `/\w\w` `/..`）は行頭から数えた区切りと位相がずれ得る。格子の当たり
// （窓の余裕はバイト）と text の表示の巨大な行の窓（窓はコードポイント）が共用する 1 本。
[[nodiscard]] std::vector<OffsetRange> vim_window_matches(std::string_view text, OffsetRange window,
                                                          const VimPattern &pattern);

// at の行の、at 以降に始まる最も左の一致（位置は本文の中のバイト・#1617）。数え始めが at なので
// vim_line_matches の区切りとは限らない。区切りの一致の始まり（vim_find_match の答え）か行頭を
// 渡せば、その区切りの一致が返る。行を写さず（piece をまたぐ行だけ vim_find_match と同じく繋ぐ）、
// 照合は at から 1 回だけ。`gn` `gN` が当たりの終わりと 1 行目の最初の当たりを引く。
// `\zs` `\ze` を含むパターン（VimPattern::bounded）だけは、照合の始まりが置き直した始まりより
// 前にありうるので、行頭からの区切りの一致のうち置き直した始まりが at 以降の最初を返す（ADR 0184
// の決定 5・照合は行の区切りの数だけ）。
[[nodiscard]] std::optional<OffsetRange> vim_match_from(const TextBuffer &text,
                                                        const VimPattern &pattern, Offset at);

// at の行の区切りの一致（vim_line_matches と同じ走査）のうち、終わりが at を越えない最後の一致
// （位置は本文の中のバイト・#1617）。Vim の searchit の後ろ向きの SEARCH_END が起点の行で数える
// 当たり（`endpos.col - 1 < start_pos.col`）で、at ちょうどの空の一致も入る。走査は行頭から 1
// 回で、 行を写さない。起点が行頭のときに起点の行を見ない規則と、ほかの行は呼ぶ側（`gn`
// `gN`）が持つ。
[[nodiscard]] std::optional<OffsetRange>
vim_last_match_ending_by(const TextBuffer &text, const VimPattern &pattern, Offset at);

// 次の一致を求める 1 本の経路（ADR 0032 の決定 3・4 の追記・ADR 0041 の決定 1）。純関数で、
// 本文・履歴・状態を持たない。確定の検索の鍵（`/` `?` の Enter・`n` `N` `*` `#`）と
// incsearch の preview の両方がこれを呼ぶ。
//
// 1 回ぶんは、前向きは request.from の次の文字以降で最初の一致、後ろ向きは request.from
// より前の最後の一致。 行ごとに 0 桁目から数え直し、必要な桁に足りない一致は終端（長さ 0 なら 1
// 文字先）まで 飛ばして数え直し、位置が行の長さに達したらその行を打ち切る（"aaaa" の /aa は 3
// 桁目、 "abc" の /.* は動かない）。一致は行をまたがない。request.wrapscan（既定は真）なら
// 本文の端を越えて起点の行まで戻り、偽なら端で止めて hit_bottom_without_match（前向き）か
// hit_top_without_match（後ろ向き）を返す（Vim の searchit・#1561）。
//
// 回数は着いた位置からもう一度探して数える（`3/x` と `/x` ＋ `3n` が同じ・実測）。wrapped は
// どれか 1 回でも折り返したら立つ。1 回でも見つからなければ pattern_not_found で、
// 呼ぶ側が E486 を出して命令ごと取り消す（実測）。
[[nodiscard]] std::expected<VimSearchHit, VimSearchNoticeKind>
vim_find_match(const TextBuffer &text, const VimPattern &pattern, const VimMatchRequest &request);
} // namespace nenenib::core
