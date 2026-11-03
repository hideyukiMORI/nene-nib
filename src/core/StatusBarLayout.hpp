#pragma once

#include "DiagnosticCounts.hpp"
#include "LayoutRect.hpp"
#include "StatusBarHit.hpp"
#include "StatusItems.hpp"

#include <array>
#include <cstdint>

namespace nenenib::core
{
// ステータスバーの寸法（docs/design/2026-09-15-look.md 第 2 節）を物理画素で表した値。
// 左はモードトグルと現在モード、右は status_items_for が作る 4 項目（ADR 0008 の決定 4・5）。
struct StatusBarLayout
{
    LayoutRect band;
    LayoutRect toggle;
    LayoutRect toggle_ordinary;
    LayoutRect toggle_vim;
    LayoutRect mode;
    std::array<LayoutRect, status_item_count> items;
    // 診断の件数（ADR 0161 の決定 4）。言語の右のいちばん右で、件数を出さないときは右端の幅 0 の
    // 矩形（ほかの項目は件数が無いときの位置・間の余白も取らない）。
    LayoutRect diagnostics;
    // 表示形式の印（ADR 0140 の決定 3）。items の最初の項目の左隣で、案内の右端（items の最初の
    // 左から 28 DIP）より右に収まる幅。右寄せで書く。
    LayoutRect display;
    std::int32_t corner_radius;
    std::int32_t segment_radius;
};

// counts は診断の件数（diagnostic_counts）。counted でなければ件数の項目は幅 0。
[[nodiscard]] StatusBarLayout status_bar_layout(std::int32_t width, std::int32_t height,
                                                std::uint32_t dpi,
                                                const DiagnosticCounts &counts) noexcept;

// 件数の項目を出さないときの配置（帯の高さだけを読む所と、診断を持たない試験）。
[[nodiscard]] StatusBarLayout status_bar_layout(std::int32_t width, std::int32_t height,
                                                std::uint32_t dpi) noexcept;

[[nodiscard]] StatusBarHit status_bar_hit(const StatusBarLayout &layout, std::int32_t x,
                                          std::int32_t y) noexcept;
} // namespace nenenib::core
