#pragma once

#include "VimPatternAtomKind.hpp"

#include <cstddef>

namespace nenenib::core
{
// 解析済みのパターンの 1 原子（ADR 0032 の決定 4）。set の区間は VimPattern が 1 本の列に
// 持ち、この値はその添字だけを指す。値は単独で妥当なので公開 aggregate（CPP-003）。
struct VimPatternAtom
{
    VimPatternAtomKind kind;
    // literal の code point。ほかの種類では 0。
    char32_t code;
    // set の `[^...]`。
    bool negated;
    // 直後の `*`（0 回以上の繰り返し）。幅の無い原子には付かない。
    bool repeated;
    // set の区間の VimPattern の列の中での始まりと個数。ほかの種類では 0。
    std::size_t first_range;
    std::size_t range_count;
    // `[...]` の集合なら真（`ignorecase` で大小を畳む）。`\d \w \s` の集合は Vim と同じく
    // 畳まない（#1041）。
    bool bracketed;
    // 大小を畳まずに比べる literal（#1147）。結合文字の code point と、結合文字が直後に続く
    // 基底の文字。Vim 9.1 の NFA は基底と結合文字の列を 1 つの原子（NFA_COMPOSING）にして
    // 畳まずに比べる（`:set ic` の `/é` は `É` に当たらない・実測）。
    bool exact;
};
} // namespace nenenib::core
