#pragma once

#include "VimScreenPlace.hpp"

namespace nenenib::core
{
// 縦の位置合わせの 1 鍵の意味（ADR 0146 の決定 2）。キャレットの行を置く所と、キャレットを行の
// 最初の非空白へ動かすか（z<CR> z. z-）。動かさない鍵（zt zz zb）は欲しい列も保つ（決定 4）。
struct VimScreenAlignment
{
    VimScreenPlace place;
    bool first_non_blank = false;
};
} // namespace nenenib::core
