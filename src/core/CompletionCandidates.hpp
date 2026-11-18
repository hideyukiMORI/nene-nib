#pragma once

#include "CompletionDirection.hpp"
#include "CompletionList.hpp"
#include "CompletionTables.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
// 候補の数の上限（ADR 0169 の決定 2）。
inline constexpr std::size_t completion_candidate_limit = 100;

// 候補を集めるときにキャレットの前と後ろへそれぞれ走る本文のバイト数（ADR 0169 の決定 2 の
// 「位置の近い順」）。本文がこの窓に収まれば Vim の順と同じで、収まらなければ窓の中を Vim の順で
// 並べた後に、窓の外にある語を語の表から語の順で足す。走るのは Ctrl-n / Ctrl-p で列を作るときの
// 1 回だけで、打鍵の道は走らない。
inline constexpr std::size_t completion_scan_bytes = std::size_t{256} * 1024U;

// content（正しい UTF-8）の caret の直前で終わる接頭辞の始まり（同じ種類の語の文字の連なり・Vim の
// get_normal_compl_info）。直前が語の文字でなければ caret（空の接頭辞）。completion_list と、開いて
// いる補完の面が「打った分がまだ 1 つの接頭辞か」を確かめる所が同じこの 1 本を読む（ADR 0190 の
// 決定 5）。
[[nodiscard]] std::size_t completion_prefix_start(std::string_view content,
                                                  std::size_t caret) noexcept;

// INSERT の Ctrl-n / Ctrl-p の候補の列（ADR 0169 の決定 2・Vim の insexpand.c の oracle の実測）。
//
// 接頭辞はキャレットの直前の 1 文字と同じ種類の語の文字の連なり（直前が語の文字でなければ空）。
// 今の文書の語は、語の頭のうち接頭辞で始まり（接頭辞が 0〜1 文字なら続く 1 文字も語の文字・
// Vim の `\<\k\k` と `\<x\k`）、打っている語（接頭辞の位置から始まる語・空の接頭辞では除かない）
// でないもの。forward はキャレットの後ろから末尾、折り返して先頭から接頭辞の手前、backward は
// その逆の順に並べる。重複は先に出た 1 つ。
// 次に他のタブの語（tables.others の順・表の中は語の順）。全部で completion_candidate_limit
// 個まで。 tables.own が無い（16 MiB 超で表を作っていない）文書では本文の語を使わない（決定 1
// の限界）。
[[nodiscard]] CompletionList completion_list(const TextBuffer &text, Offset caret,
                                             CompletionDirection direction,
                                             const CompletionTables &tables);
} // namespace nenenib::core
