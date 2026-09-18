#pragma once

#include "Offset.hpp"

#include <string>

namespace nenenib::core
{
// p / P（ADR 0015 の決定 4）。engine が「どこに」「何を」「貼ったあとキャレットをどこに」を決め、
// controller は utf8 の LF を文書の改行に直して at に入れ、caret に置く。
// utf8 と caret はどちらも LF で数えた値で、CRLF の文書では controller が同じ 1 か所でずらす。
struct VimPutString
{
    Offset at;
    std::string utf8;
    Offset caret;
};
} // namespace nenenib::core
