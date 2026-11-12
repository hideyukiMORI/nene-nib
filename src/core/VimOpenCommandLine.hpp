#pragma once

#include "Offset.hpp"

#include <optional>
#include <string>

namespace nenenib::core
{
// `:` の入力行を開く効果（ADR 0022 の決定 2 と追補 #1526）。range は行の先に置く範囲（VISUAL の
// `'<,'>`・回数の `.,.+N`・Vim の op_colon / nv_colon）で、空なら何も置かない。caret は VISUAL を
// 抜けて置くキャレット（Vim の oap->start）で、NORMAL の `:` では値なし。
struct VimOpenCommandLine
{
    std::string range;
    std::optional<Offset> caret = std::nullopt;
};
} // namespace nenenib::core
