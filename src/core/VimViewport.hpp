#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::core
{
// 1 回の Vim の鍵が読む表示領域。所有者は application の ScrollState であり、これは
// engine に渡す値だけを写した入力（ADR 0019 の決定 1）。先頭行の画面行もスクロールに依るので
// 同じ値に畳む（ADR 0129 の決定 1）。columns は本文の欄に入る升の数、left_column は今の横位置を
// 升で数えた leftcol（0 始まり・キャレットの行の仮想桁）。横に送らない文書（wrap on の普通の行・
// 格子）では columns が 0 で、横送りの鍵は何もしない（ADR 0145 の決定 2）。
struct VimViewport
{
    LineNumber first_visible;
    std::size_t visible_lines;
    std::size_t first_row;
    std::size_t columns = 0;
    std::size_t left_column = 0;
};
} // namespace nenenib::core
