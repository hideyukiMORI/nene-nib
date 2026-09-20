#pragma once

#include "EditBoundary.hpp"
#include "Offset.hpp"

#include <string>

namespace nenenib::core
{
// p / P と o / O（ADR 0028）。engine が位置・本文・挿入後のcaret・履歴境界を決め、
// controller は utf8 の LF を文書の改行に直して at に入れ、caret に置く。
// utf8 と caret はどちらも LF で数えた値で、CRLF の文書では controller が同じ 1 か所でずらす。
struct VimInsertAt
{
    Offset at;
    std::string utf8;
    Offset caret;
    EditBoundary boundary;
};
} // namespace nenenib::core
