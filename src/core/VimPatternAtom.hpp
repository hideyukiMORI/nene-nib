#pragma once

#include "VimPatternAtomKind.hpp"
#include "VimPatternRepeat.hpp"

#include <cstddef>

namespace nenenib::core
{
// 解析済みのパターンの 1 原子（ADR 0032 の決定 4）。set の区間は VimPattern が 1 本の列に
// 持ち、この値はその添字だけを指す。値は単独で妥当なので公開 aggregate（CPP-003）。
struct VimPatternAtom
{
    VimPatternAtomKind kind;
    // literal と cluster の（基底の）code point。ほかの種類では 0。
    char32_t code;
    // set の `[^...]`。
    bool negated;
    // 原子に付く量（ADR 0184 の決定 2）。once のほかは直後の `*` か、parse が開いた量の写しの
    // 残り。幅の無い原子は once だけ。
    VimPatternRepeat repeat;
    // set の区間の VimPattern の列の中での始まりと個数。composing と cluster は結合文字 1 つずつを
    // 幅 1 の区間（first == last）として持つ（#1506）。ほかの種類では 0。
    std::size_t first_range;
    std::size_t range_count;
    // `[...]` の集合なら真（`ignorecase` で大小を畳む）。`\d \w \s` の集合は Vim と同じく
    // 畳まない（#1041）。
    bool bracketed;
    // 大小を畳まずに比べる literal（#1147）。結合文字の code point と、結合文字が直後に続く
    // 繰り返しの無い基底の文字。Vim 9.1 の NFA は基底と結合文字の列を 1 つの原子
    // （NFA_COMPOSING）にして畳まずに比べる（`:set ic` の `/é` は `É` に当たらない・実測）。
    // `e*` + 結合文字の `e` は別の原子として畳む（Vim の読み・実測とは合わない残差・ADR 0032）。
    bool exact;
};
} // namespace nenenib::core
