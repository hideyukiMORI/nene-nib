#pragma once

#include "FontSize.hpp"
#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 本文の帯の寸法（docs/design/2026-09-15-look.md 第 2 節）を物理画素で表した値。
// 見えている行数もここが決める。窓（何行送るか）と描画（どこに描くか）が同じ数を使う（ARC-001）。
struct BodyLayout
{
    LayoutRect band;
    LayoutRect gutter;
    LayoutRect content;
    std::int32_t line_height;
    std::int32_t caret_width;
    std::size_t visible_lines;
    // 先頭の画面行が本文の上端（content.top）より上へはみ出している物理画素（#1898）。0 以上
    // line_height 未満で、置くのは scrolled_body の 1 か所。画面行の上端（body_row_top）と点の
    // 画面行（body_row_at）が同じ値を読み、描く所と当てる所が同じずらしになる（ARC-001）。
    std::int32_t scrolled = 0;
};

[[nodiscard]] BodyLayout body_layout(std::int32_t width, std::int32_t height, std::uint32_t dpi,
                                     FontSize font_size) noexcept;

// 本文の帯（タブの帯の下からステータスバーの上まで・幅は client の全幅）。字の大きさに依らない。
// body_layout とエクスプローラーの面の縦（ADR 0162 の決定 2）が共用する 1 本。
[[nodiscard]] LayoutRect body_band(std::int32_t width, std::int32_t height,
                                   std::uint32_t dpi) noexcept;

// 帯 band の中に行番号の欄・本文・見えている行数を置く。body_layout と枠の配置（pane_layouts・
// ADR 0148 の決定 1）が共用する 1 本で、帯の左端と幅は band のものを使う。
[[nodiscard]] BodyLayout body_layout_in(LayoutRect band, std::uint32_t dpi,
                                        FontSize font_size) noexcept;

[[nodiscard]] LayoutRect body_line_rect(const BodyLayout &layout, std::size_t index) noexcept;

// 端数 fraction（1/scroll_fraction_units 行・ScrollState::fraction）を行の高さの物理画素に丸めて
// layout に置く（#1898）。切り捨てなので 0 は 0 のまま、どの端数も line_height 未満。
[[nodiscard]] BodyLayout scrolled_body(BodyLayout layout, std::size_t fraction) noexcept;

// 画面行 row（先頭の画面行が 0）の上端の物理画素。ずらし（scrolled）を引いた値。
[[nodiscard]] std::int32_t body_row_top(const BodyLayout &layout, std::int64_t row) noexcept;

// 点の縦 y の画面行。本文の上端より上は 0（クリックとドラッグの当たり判定の 1 本）。
[[nodiscard]] std::int64_t body_row_at(const BodyLayout &layout, std::int32_t y) noexcept;
} // namespace nenenib::core
