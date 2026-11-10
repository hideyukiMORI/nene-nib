#pragma once

#include "FontSize.hpp"
#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// ミニマップの面の配置（ADR 0171 の決定 2）。物理画素。panel は本文の帯の高さの右端の面（出て
// いなければ幅 0）、body は面の幅だけ右を縮めた client で、本文の枠はこれを pane_layouts へ渡して
// 求める（ADR 0148 の 1 本はそのまま・2 枠のときは右の枠の右に出る）。rows は面に入る行の数
// （1 行 = minimap_row_pixels）。どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct MinimapLayout
{
    LayoutRect panel;
    LayoutRect body;
    std::size_t rows;
};

// 面の幅は min(80 DIP, client の幅)。縦は本文の帯（タブの帯の下からステータスバーの上まで）。
// shown が偽なら幅は 0 で body は client のまま。分岐せず幅に shown を掛ける（決定 1・オフの道に
// 分岐を足さない）。
[[nodiscard]] MinimapLayout minimap_layout(LayoutRect client, std::uint32_t dpi, FontSize font_size,
                                           bool shown) noexcept;
} // namespace nenenib::core
