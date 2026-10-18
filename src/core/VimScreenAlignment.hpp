#pragma once

#include "VimAlignedLine.hpp"
#include "VimScreenPlace.hpp"

namespace nenenib::core
{
// 縦の位置合わせの 1 鍵の意味（ADR 0146 の決定 2）。キャレットの行を置く所と、キャレットを行の
// 最初の非空白へ動かすか（z<CR> z. z- z+ z^）。動かさない鍵（zt zz zb）は欲しい列も保つ（決定 4）。
// 合わせる行の選び方（z+ z^ は窓の外の行・決定 9）。
struct VimScreenAlignment
{
    VimScreenPlace place;
    bool first_non_blank = false;
    VimAlignedLine line = VimAlignedLine::caret;
};
} // namespace nenenib::core
