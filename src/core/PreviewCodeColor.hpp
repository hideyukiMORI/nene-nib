#pragma once

#include "Offset.hpp"
#include "SyntaxRole.hpp"

namespace nenenib::core
{
// 囲みのコードの要素の text の中の構文の色の半開区間 [begin, end)（#1858）。位置は要素の text の
// バイト位置で、要素の中では昇順で重ならない。区間の無い所は本文の文字の色。役割は本文の字句器
// （lex_line）の答えのまま。比較は非メンバー（CPP-003）。
struct PreviewCodeColor
{
    Offset begin;
    Offset end;
    SyntaxRole role;
};

[[nodiscard]] constexpr bool operator==(const PreviewCodeColor &left,
                                        const PreviewCodeColor &right) noexcept
{
    return left.begin == right.begin && left.end == right.end && left.role == right.role;
}
} // namespace nenenib::core
