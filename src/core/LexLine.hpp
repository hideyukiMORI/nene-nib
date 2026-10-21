#pragma once

#include "ColorSpan.hpp"
#include "LexState.hpp"

#include <vector>

namespace nenenib::core
{
// lex_line の答え（#815・ADR 0156 の決定 4）。1 行の色の区間と、次の行へ渡す状態。
struct LexLine
{
    std::vector<ColorSpan> spans;
    LexState out;
};
} // namespace nenenib::core
