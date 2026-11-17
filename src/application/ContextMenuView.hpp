#pragma once

#include "ContextMenuRow.hpp"
#include "ContextMenuShape.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::application
{
// 本文の右クリックのメニューの表示値（ADR 0078 の追補 #1051）。x と y は開く点（クライアント座標の
// 物理画素）、highlighted は選んでいる行（上下の鍵とマウスを載せた行・使える行だけ）。配置は ui が
// 名前と鍵の幅を測って core::context_menu_layout に渡す。
struct ContextMenuView
{
    std::int32_t x;
    std::int32_t y;
    std::array<ContextMenuRow, core::context_menu_size> rows;
    std::optional<std::size_t> highlighted;
};
} // namespace nenenib::application
