#pragma once

#include "LineNumber.hpp"
#include "VimLineRows.hpp"

#include <cstddef>
#include <span>

namespace nenenib::core
{
// Vim の update_topline が読む窓（#1178・ADR 0019 の追補）。top は窓の先頭の行（Vim の
// w_topline）、height は窓の画面行の数、margin は余白（設定の scrolloff・切り詰めない値）。rows は
// 行ごとの画面行の数（Vim の plines・行の順）で、application が文字組みから借りる。空の道（文字組み
// の無い道）と載っていない行は 1 論理行 = 1 画面行。
struct VimToplineWindow
{
    LineNumber top;
    std::size_t height;
    std::size_t margin;
    std::span<const VimLineRows> rows{};
};
} // namespace nenenib::core
