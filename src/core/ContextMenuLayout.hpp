#pragma once

#include "ContextMenuShape.hpp"
#include "LayoutRect.hpp"

#include <array>
#include <cstdint>

namespace nenenib::core
{
// メニューの矩形（ADR 0078 の追補 #1051）。描画と当たりの判定が同じ値を読む。rows は項目の順の
// 行、separators は区切り線（高さ 1 DIP）。
struct ContextMenuLayout
{
    LayoutRect panel;
    std::array<LayoutRect, context_menu_size> rows;
    std::array<LayoutRect, context_menu_separator_count> separators;
    std::uint32_t dpi;
};
} // namespace nenenib::core
