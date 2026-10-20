#pragma once

#include "Offset.hpp"
#include "TextBuffer.hpp"

#include <optional>

namespace nenenib::core
{
// `%` の着地（ADR 0012 の追記・Issue #767）。キャレットの文字から行の終わりへ向かって最初に
// 見つかる `( ) [ ] { }`（'matchpairs' の既定）の相手を、入れ子を数えて探す（Vim の
// findmatchlimit）。'cpoptions' に `%` が無い既定と同じに、行の引用符の数が偶数なら走査が越えた
// `"` の内側の括弧は数えず、`'x'` `'\x'` の文字は飛ばし、奇数個の `\` が前にある括弧は始まりの
// 括弧と同じときだけ数える。行の中に括弧が無い・相手が無いときは nullopt。
// `#if` と `/* */` の対応と、行末の `\` で続く引用符は持たない（ADR 0152 の限界）。
[[nodiscard]] std::optional<Offset> vim_matching_pair(const TextBuffer &text, Offset caret);
} // namespace nenenib::core
