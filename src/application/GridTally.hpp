#pragma once

#include <cstddef>

namespace nenenib::application
{
// 格子の見えている画面行を作った 1 枠ぶんの数（ADR 0140 の決定 5・#839）。scanned は升を数えるのに
// 歩いた本文の塊の数（EditorFrame::grid_scanned）、searched は当たりの照合に読んだバイト数
// （grid_searched）、lexed は字句器へ通した行の数（syntax_lexed・ADR 0156 の決定 8）。
struct GridTally
{
    std::size_t scanned = 0;
    std::size_t searched = 0;
    std::size_t lexed = 0;
};
} // namespace nenenib::application
