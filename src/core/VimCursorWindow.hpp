#pragma once

#include "LineNumber.hpp"
#include "VimLineRows.hpp"

#include <cstddef>
#include <span>

namespace nenenib::core
{
// Vim の cursor_correct が読む窓（#1099・ADR 0019 の追補）。top は窓の先頭の行、below は窓に
// 全部は入らない最初の行（Vim の w_botline・本文の末尾まで見えていれば行数 + 1）、height は窓の
// 画面行の数、margin は余白（設定の scrolloff を controller が VimEditorView.scroll_margin
// に写した値）。rows は文字組みの道が借りる行ごとの画面行の数（VimLayoutView.lines・行の順）で、
// 余白を詰めるときに Vim の plines として足す（#1141）。空の道（文字組みの無い道）と載っていない
// 行は 1 論理行 = 1 画面行。
struct VimCursorWindow
{
    LineNumber top;
    LineNumber below;
    std::size_t height;
    std::size_t margin;
    std::span<const VimLineRows> rows{};
};
} // namespace nenenib::core
