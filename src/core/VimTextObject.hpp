#pragma once

#include <cstdint>

namespace nenenib::core
{
// テキストオブジェクトの閉じた一覧（ADR 0031 の決定 1）。鍵から表で写す。
// b は paren、B は brace、閉じ括弧の鍵は開き括弧と同じ行（Vim 9.1 で実測）。
// ip / ap は段落、is / as は文（Issue #729 の追補）。it / at はまだ入れない。
enum class VimTextObject : std::uint8_t
{
    word,
    big_word,
    double_quote,
    single_quote,
    backtick,
    paren,
    brace,
    bracket,
    angle,
    paragraph,
    sentence
};
} // namespace nenenib::core
