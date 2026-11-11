#pragma once

#include "LineNumber.hpp"
#include "VimLineRows.hpp"

#include <cstddef>
#include <span>

namespace nenenib::core
{
// Vim の update_topline が読む窓（#1178・ADR 0019 の追補）。top は窓の先頭の行（Vim の
// w_topline）、height は窓の画面行の数、margin は余白（設定の scrolloff・切り詰めない値）。rows は
// 行ごとの画面行の数（Vim の plines・行の順）で、application が文字組みから借りる。asks が真の道
// （文字組みの道）では、載っていない行を読むと vim_topline_updated がその行を問いとして返す。偽の道
// （文字組みの無い道）では載っていない行は 1 論理行 = 1 画面行。
// top は本文の行数を越えてよい（行単位の削除の後に残る Vim の w_topline・#1230）。
// walks が真なら、キャレットは INSERT で打った本文を 1 文字ずつ下へ歩いて来た（Vim の edit は
// 1 文字ごとに update_topline を呼ぶ・#1363）。
struct VimToplineWindow
{
    LineNumber top;
    std::size_t height;
    std::size_t margin;
    std::span<const VimLineRows> rows{};
    bool asks = false;
    bool walks = false;
};
} // namespace nenenib::core
