#pragma once

#include "Appearance.hpp"
#include "VimSearchHighlight.hpp"

namespace nenenib::core
{
// Ex の評価が読む、設定に載せない窓の状態（OS の明暗と、Vim の状態の hlsearch・incsearch）。
// `:colorscheme` は明暗を、`:set hls!` `invhls` の反転は今の値を、`hls&` `is&` は既定値（値を
// 与えない初期値）を読む（#1525 のレビュー V-1552 の L4）。
struct ExWindowState
{
    Appearance system_appearance;
    VimSearchHighlight highlight = vim_default_highlight;
    bool incsearch = vim_default_incsearch;
};
} // namespace nenenib::core
