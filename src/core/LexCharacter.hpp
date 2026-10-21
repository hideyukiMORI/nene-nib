#pragma once

#include <cstdint>

namespace nenenib::core
{
// C / C++ の字句器が 1 バイトを振り分ける種類（#815・ADR 0156 の決定 6）。バイト → 種類と
// 種類 → 字句の関数は LexCpp.cpp の 2 つの constexpr の表で、値を足すと表の static_assert
// が落ちる。 0x80 以上（UTF-8 の多バイト）は other で、分類しない。
enum class LexCharacter : std::uint8_t
{
    space,
    word,
    digit,
    quote,
    apostrophe,
    slash,
    hash,
    operators,
    other
};
} // namespace nenenib::core
