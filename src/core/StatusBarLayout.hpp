#pragma once

#include "LayoutRect.hpp"
#include "StatusBarHit.hpp"
#include "StatusItems.hpp"

#include <array>
#include <cstdint>

namespace nenenib::core
{
// ステータスバーの寸法（docs/design/2026-09-15-look.md 第 2 節）を物理画素で表した値。
// 左はモードトグルと現在モード、右は status_items_for が作る 3 項目（ADR 0008 の決定 4・5）。
struct StatusBarLayout
{
    LayoutRect band;
    LayoutRect toggle;
    LayoutRect toggle_ordinary;
    LayoutRect toggle_vim;
    LayoutRect mode;
    std::array<LayoutRect, status_item_count> items;
    std::int32_t corner_radius;
    std::int32_t segment_radius;
};

[[nodiscard]] StatusBarLayout status_bar_layout(std::int32_t width, std::int32_t height,
                                                std::uint32_t dpi) noexcept;

[[nodiscard]] StatusBarHit status_bar_hit(const StatusBarLayout &layout, std::int32_t x,
                                          std::int32_t y) noexcept;
} // namespace nenenib::core
