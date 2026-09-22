#pragma once

#include <cstdint>

namespace nenenib::core
{
// 照合器が扱う原子の閉じた一覧（ADR 0032 の決定 4）。magic の部分集合だけで、
// 未対応の構文は原子にならず閉じた失敗（VimPatternFailure）で拒否される。
enum class VimPatternAtomKind : std::uint8_t
{
    // 1 つの code point そのもの。
    literal,
    // `.`（改行以外の 1 文字。一致は行をまたがない）。
    any,
    // `[...]` `[^...]` と `\d \D \w \W \s \S`。
    set,
    // `^`（パターンの先頭でだけ）。
    line_start,
    // `$`（パターンの末尾でだけ）。
    line_end,
    // `\<`。
    word_start,
    // `\>`。
    word_end
};
} // namespace nenenib::core
