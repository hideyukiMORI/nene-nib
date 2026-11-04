#pragma once

#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "TextBuffer.hpp"

#include <optional>
#include <span>

namespace nenenib::core
{
// `%` の着地（ADR 0012 の追記・Issue #767）。キャレットの文字から行の終わりへ向かって最初に
// 見つかる `( ) [ ] { }`（'matchpairs' の既定）の相手を、入れ子を数えて探す（Vim の
// findmatchlimit）。'cpoptions' に `%` が無い既定と同じに、行の引用符の数が偶数なら走査が越えた
// `"` の内側の括弧は数えず、`'x'` `'\x'` の文字は飛ばし、奇数個の `\` が前にある括弧は始まりの
// 括弧と同じときだけ数える。行の中に括弧が無い・相手が無いときは nullopt。
// `#if` と `/* */` の対応と、行末の `\` で続く引用符は持たない（ADR 0152 の限界）。
// 探索は matched_bracket の 1 本で、窓は本文の全体・除く区間は無し。
[[nodiscard]] std::optional<Offset> vim_matching_pair(const TextBuffer &text, Offset caret);

// bracket の位置の括弧（`( ) [ ] { }`）の相手（ADR 0164 の決定 2・3・Issue #999）。
// `%` と同じ走査で、本文の位置 window [begin, end) の外へ出たら探さずに nullopt
// （全文を走らない）。skipped は昇順で重ならない本文の位置の区間で、その中の
// 括弧は数えない（構文の文字列とコメント）。bracket が括弧でない・window の外・
// 相手が無いときも nullopt。
[[nodiscard]] std::optional<Offset> matched_bracket(const TextBuffer &text, Offset bracket,
                                                    OffsetRange window,
                                                    std::span<const OffsetRange> skipped);
} // namespace nenenib::core
