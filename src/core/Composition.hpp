#pragma once

#include "CompositionClause.hpp"
#include "Offset.hpp"

#include <string>
#include <vector>

namespace nenenib::core
{
// 変換中の文字列（ADR 0014 の決定 2）。本文ではないので TextBuffer にも EditHistory にも入らず、
// EditorState が std::optional で本文の外に持つ。cursor は utf8 の中のバイト位置
// （GCS_CURSORPOS を写したもの）で、本文のバイト位置ではない。
struct Composition
{
    std::string utf8;
    std::vector<CompositionClause> clauses;
    Offset cursor;
};

// 文節の列を「描く下線の列」に畳む純関数（決定 7）。IME が文節を返さないとき・範囲が
// 変換中の文字列からはみ出すとき・文節の間に隙間が空くときも、必ず utf8 の全体を
// 隙間なく覆う並びを返す。矩形に直すのは renderer の仕事で、ここは種類と範囲だけを決める。
[[nodiscard]] std::vector<CompositionClause> composition_underlines(const Composition &composition);
} // namespace nenenib::core
