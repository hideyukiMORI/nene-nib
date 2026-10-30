#pragma once

#include "FontSize.hpp"
#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::core
{
// エクスプローラーの面の配置（ADR 0162 の決定 2）。物理画素。panel は本文の帯の
// 高さの左の面（消えていれば値なし）、body は面の幅だけ左を縮めた client で、
// 本文の枠はこれを pane_layouts へ渡して求める（ADR 0148 の 1 本はそのまま）。
// line_height は本文の行の高さ、indent は 1 段の字下げ（12 DIP）、marker は
// 印 ▸ / ▾ の升の幅（display_width の桁 × 字の大きさ 1 つ分）、padding は面の
// 左右の余白（8 DIP）、border は面と本文の境界の太さ（1 DIP・1 画素以上）。
// どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct ExplorerLayout
{
    std::optional<LayoutRect> panel;
    LayoutRect body;
    std::int32_t line_height;
    std::int32_t indent;
    std::int32_t marker;
    std::int32_t padding;
    std::int32_t border;
};

// 面の幅は min(240 DIP, client の幅の 40%)（切り捨て）。縦は本文の帯（タブの帯の下からステータス
// バーの上まで）。shown が偽なら panel は値なしで body は client のまま。
[[nodiscard]] ExplorerLayout explorer_layout(LayoutRect client, std::uint32_t dpi,
                                             FontSize font_size, bool shown) noexcept;

// 面の高さ（本文の帯の全高）に入る行数（先頭のルート名の行を含む・切り捨て・#1125）。面が
// 出ているかに依らず、本文の分割（ADR 0148）にも依らない。行の高さが 0 以下なら 0。
[[nodiscard]] std::size_t explorer_panel_lines(LayoutRect client, std::uint32_t dpi,
                                               FontSize font_size) noexcept;
} // namespace nenenib::core
