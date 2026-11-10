#pragma once

#include "ExplorerWidth.hpp"
#include "FontSize.hpp"
#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

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

// 使う人が決める幅の最小（DIP・D78）と、設定の値として読む上限（DIP・4 桁）。
inline constexpr std::int32_t explorer_width_minimum_dips = 120;
inline constexpr std::int32_t explorer_width_limit_dips = 9999;

// 面の幅は explorer_panel_width の 1 本。縦は本文の帯（タブの帯の下からステータスバーの上まで）。
// shown が値なしなら panel は値なしで body は client のまま（幅の計算はしない）。
[[nodiscard]] ExplorerLayout explorer_layout(LayoutRect client, std::uint32_t dpi,
                                             FontSize font_size,
                                             std::optional<ExplorerWidth> shown) noexcept;

// 面の幅（物理画素・ADR 0162 の決定 2 と追補 D78）。width.dips が無ければ既定の式
// min(240 DIP, client の 40%)（切り捨て・今までと同じ値）。有れば範囲に切り詰める: 最小 120 DIP、
// 最大は client の 60%（切り捨て）と「本文に 200 DIP を残す幅」の小さい方。最大が最小より小さい
// （窓が狭い）ときは最大が勝ち、0 を下回らない。覚えた値は書き換えず、描く幅だけを切り詰める。
[[nodiscard]] std::int32_t explorer_panel_width(std::int32_t client_width, std::uint32_t dpi,
                                                ExplorerWidth width) noexcept;

// 境のドラッグで面の右端を panel_pixels（client の左からの物理画素）に置いたときに覚える幅
// （DIP）。explorer_panel_width と同じ範囲に切り詰めた画素を DIP へ直し（切り捨て）、
// [explorer_width_minimum_dips, explorer_width_limit_dips] に収める（設定の値として常に読める）。
[[nodiscard]] std::int32_t explorer_dragged_width(std::int32_t client_width, std::uint32_t dpi,
                                                  std::int32_t panel_pixels) noexcept;

// 面と本文の境の当たり（左右 3 DIP・1 画素以上・縦は面の帯）。面が消えているか幅が 0 なら偽。
// 面の行の当たり（panel の中）より先に引く。
[[nodiscard]] bool explorer_divider_hit(const ExplorerLayout &layout, std::uint32_t dpi,
                                        std::int32_t x, std::int32_t y) noexcept;

// 設定 explorer_width の値（10 進・先頭の 0 なし・explorer_width_minimum_dips〜
// explorer_width_limit_dips）。空と形の違う値は値なし（既定の式・D78 の「壊れた値は既定」）。
[[nodiscard]] std::optional<std::int32_t> explorer_width_of(std::string_view text) noexcept;

// 面の高さ（本文の帯の全高）に入る行数（先頭のルート名の行を含む・切り捨て・#1125）。面が
// 出ているかに依らず、本文の分割（ADR 0148）にも依らない。行の高さが 0 以下なら 0。
[[nodiscard]] std::size_t explorer_panel_lines(LayoutRect client, std::uint32_t dpi,
                                               FontSize font_size) noexcept;
} // namespace nenenib::core
