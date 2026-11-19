#pragma once

#include "ContextMenuShape.hpp"
#include "LayoutRect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// メニューの矩形（ADR 0078 の追補 #1051 / #1790）。描画と当たりの判定が同じ値を読む。rows は項目の
// 順の行（先頭の row_count 個が使う行）、separators は区切り線（高さ 1 DIP・先頭の separator_count
// 個）。
struct ContextMenuLayout
{
    LayoutRect panel;
    std::array<LayoutRect, context_menu_row_capacity> rows;
    std::array<LayoutRect, context_menu_separator_capacity> separators;
    std::uint32_t dpi;
    std::size_t row_count;
    std::size_t separator_count;
};
} // namespace nenenib::core
