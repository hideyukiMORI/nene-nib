#pragma once

#include <cstdint>

namespace nenenib::core
{
// 縦の位置合わせで合わせる行の選び方（ADR 0146 の決定 3・9）。caret は回数の行か今の行
// （zt zz zb z<CR> z. z-）。below_window は回数が無ければ窓の 1 つ下の行（z+）。above_window は
// 回数が無ければ窓の 1 つ上の行、回数があればその行を下端に置いたときの先頭の行（z^）。
enum class VimAlignedLine : std::uint8_t
{
    caret,
    below_window,
    above_window
};
} // namespace nenenib::core
