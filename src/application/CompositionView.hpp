#pragma once

#include "CompositionClause.hpp"
#include "Offset.hpp"

#include <string>
#include <vector>

namespace nenenib::application
{
// 変換中の文字列の表示値（ARC-011）。UI はキャレットの位置にこれを差し込んで描くだけで、
// 本文には触らない。underlines は core::composition_underlines が畳んだ並びで、
// utf8 の全体を隙間なく覆う。cursor は utf8 の中のバイト位置（ADR 0014 の決定 7）。
struct CompositionView
{
    std::string utf8;
    std::vector<core::CompositionClause> underlines;
    core::Offset cursor;
};
} // namespace nenenib::application
