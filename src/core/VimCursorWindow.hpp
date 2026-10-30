#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::core
{
// Vim の cursor_correct が読む窓（#1099・ADR 0019 の追補）。top は窓の先頭の行、below は窓に
// 全部は入らない最初の行（Vim の w_botline・本文の末尾まで見えていれば行数 + 1）、height は窓の
// 画面行の数、margin は余白（設定の scrolloff を controller が VimEditorView.scroll_margin
// に写した値）。
struct VimCursorWindow
{
    LineNumber top;
    LineNumber below;
    std::size_t height;
    std::size_t margin;
};
} // namespace nenenib::core
