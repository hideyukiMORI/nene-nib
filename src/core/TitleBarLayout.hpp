#pragma once

#include "LayoutRect.hpp"
#include "TitleBarHit.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 自前タイトルバーの寸法（docs/design/2026-09-15-look.md 第 2 節）を物理画素で表した値。
// OS を知らない純関数が作り、窓手続きと描画はこれを写すだけ（ADR 0008 の決定 5）。
struct TitleBarLayout
{
    LayoutRect band;
    LayoutRect tabs;
    LayoutRect add_tab;
    LayoutRect minimize;
    LayoutRect maximize;
    LayoutRect close;
    std::int32_t tab_width;
    std::int32_t tab_gap;
    std::int32_t corner_radius;
    std::int32_t underline;
    std::int32_t glyph;
    std::size_t tab_count;
};

[[nodiscard]] TitleBarLayout title_bar_layout(std::int32_t width, std::uint32_t dpi,
                                              std::size_t tab_count) noexcept;

[[nodiscard]] LayoutRect tab_rect(const TitleBarLayout &layout, std::size_t index) noexcept;

[[nodiscard]] TitleBarHit title_bar_hit(const TitleBarLayout &layout, std::int32_t x,
                                        std::int32_t y) noexcept;
} // namespace nenenib::core
