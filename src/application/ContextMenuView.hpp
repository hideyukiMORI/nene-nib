#pragma once

#include "ContextMenuKind.hpp"
#include "ContextMenuRow.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nenenib::application
{
// 右クリックのメニューの表示値（ADR 0078 の追補 #1051 / #1790）。x と y は開く点（クライアント座標
// の物理画素）、kind はメニューの種類（本文かタブか・行と区切り線の並び）、rows は項目の順の行、
// highlighted は選んでいる行（上下の鍵とマウスを載せた行・使える行だけ）。配置は ui が名前と鍵の
// 幅を測って core::context_menu_layout に渡す。
struct ContextMenuView
{
    std::int32_t x;
    std::int32_t y;
    core::ContextMenuKind kind;
    std::vector<ContextMenuRow> rows;
    std::optional<std::size_t> highlighted;
};
} // namespace nenenib::application
