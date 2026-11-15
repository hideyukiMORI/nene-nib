#pragma once

#include <cstddef>

namespace nenenib::core
{
// 1 行の中の一致（半開区間・行の先頭からのバイト位置）。長さ 0 の一致では begin == end。
// 本文の中の位置ではないので Offset ではない（単位を型で持つ・CPP-001）。
// begin と end は `\zs` `\ze` で置き直した当たり（ADR 0184 の決定 5）で、着地・塗り・置き換え・
// 数え・`gn` の範囲はどれもこの 2 欄だけを読む。whole_begin と whole_end はパターン全体が当たった
// 範囲（置き直す前）で、群の範囲を引く道（VimPatternTrace）だけが読む。`\zs` `\ze` を含まない
// パターンと群の範囲の値では 4 欄の組は begin == whole_begin・end == whole_end。
struct VimPatternMatch
{
    std::size_t begin;
    std::size_t end;
    std::size_t whole_begin;
    std::size_t whole_end;
};
} // namespace nenenib::core
