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
};
} // namespace nenenib::core
