#pragma once

#include "ExRange.hpp"
#include "ExSubstituteSource.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace nenenib::core
{
// `:s` の族の 1 命令（ADR 0168 の決定 1〜3・段 1）。evaluate_ex が字面だけを読んだ値で、範囲を行に
// 解き、前の置換と前のパターンを引き、本文を置き換えるのは engine の vim_substituted の 1 本。
// range が空なら今の行。pattern と replacement は source が typed のときだけ読む（pattern
// は区切りの 間の字面のまま・空なら最後に使ったパターン・replacement は `\` も `~`
// もそのまま）。delimiter は typed の区切りの文字。keep_flags は flags の先頭の
// `&`（前のフラグを保つ）、flags はその後の フラグの文字（`g` `c` `n` `e` `r` `i` `I` だけ・`p` `#`
// `l` は evaluate_ex が断る）、count は 末尾の回数（0 から 2147483646・0 は engine が `e`
// で決める）。どの値も単独で妥当なので公開 aggregate（CPP-003）。
struct ExSubstituteRequest
{
    std::optional<ExRange> range;
    ExSubstituteSource source;
    char delimiter;
    std::string pattern;
    std::string replacement;
    bool keep_flags;
    std::string flags;
    std::optional<std::size_t> count;
};
} // namespace nenenib::core
