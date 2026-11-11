#pragma once

#include "ExplorerRowParts.hpp"
#include "ExplorerWidth.hpp"
#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// エクスプローラーの面の配置（ADR 0162 の決定 2・採用案 B の追補 #1390・施主決定 D81）。物理画素。
// panel は本文の帯の高さの左の面（消えていれば値なし）、body は面の幅だけ左を縮めた client で、
// 本文の枠はこれを pane_layouts へ渡して求める（ADR 0148 の 1 本はそのまま）。寸法はどれも DIP の
// 固定値で、本文の字の大きさ（fontsize）に追従しない。line_height は行の高さ（22 DIP）、
// root_height は先頭のルート名の行の高さ（30 DIP）、indent は 1 段の字下げ（12 DIP）、cell は
// 山形と印の升（16 DIP）、inset は行の丸い面の左右の内側（6 DIP）、padding は丸い面の中の左右の
// 余白（4 DIP）、root_padding はルート名の左右の余白（12 DIP）、gap は印と名前の間（6 DIP）、
// radius は行の丸い面の角（5 DIP）、corner は本文の帯の左上の角（8 DIP）、dot は未保存の丸の径
// （6 DIP）、dot_inset は丸の右端と丸い面の右端の間（14 DIP）、border は面と本文の境界の太さ
// （1 DIP・1 画素以上）。どの欄も単独で妥当な値なので公開
// aggregate（CPP-003）。
struct ExplorerLayout
{
    std::optional<LayoutRect> panel;
    LayoutRect body;
    std::int32_t line_height;
    std::int32_t root_height;
    std::int32_t indent;
    std::int32_t cell;
    std::int32_t inset;
    std::int32_t padding;
    std::int32_t root_padding;
    std::int32_t gap;
    std::int32_t radius;
    std::int32_t corner;
    std::int32_t dot;
    std::int32_t dot_inset;
    std::int32_t border;
};

// 使う人が決める幅の最小（DIP・D78）と、設定の値として読む上限（DIP・4 桁）。
inline constexpr std::int32_t explorer_width_minimum_dips = 120;
inline constexpr std::int32_t explorer_width_limit_dips = 9999;

// 面の幅は explorer_panel_width の 1 本。縦は本文の帯（タブの帯の下からステータスバーの上まで）。
// shown が値なしなら panel は値なしで body は client のまま（幅の計算はしない）。
[[nodiscard]] ExplorerLayout explorer_layout(LayoutRect client, std::uint32_t dpi,
                                             std::optional<ExplorerWidth> shown) noexcept;

// 面の幅（物理画素・ADR 0162 の決定 2 と追補 D78）。width.dips が無ければ既定の式
// min(240 DIP, client の 40%)（切り捨て・今までと同じ値）。有れば範囲に切り詰める: 最小 120 DIP、
// 最大は client の 60%（切り捨て）と「本文に 200 DIP を残す幅」の小さい方。最大が最小より小さい
// （窓が狭い）ときは最大が勝ち、0 を下回らない。覚えた値は書き換えず、描く幅だけを切り詰める。
[[nodiscard]] std::int32_t explorer_panel_width(std::int32_t client_width, std::uint32_t dpi,
                                                ExplorerWidth width) noexcept;

// 使う人が決める幅（DIP）を [explorer_width_minimum_dips, explorer_width_limit_dips] に収める 1 本
// （D78・Issue #1380）。ドラッグの幅を求める所と、意図 ResizeExplorer を受ける controller の入口が
// 共用し、設定に書く値は常に自分の読み explorer_width_of で読める。INT32_MAX や負の値でも溢れない。
[[nodiscard]] std::int32_t explorer_width_clamped(std::int32_t dips) noexcept;

// 境のドラッグで面の右端を panel_pixels（client の左からの物理画素）に置いたときに覚える幅
// （DIP）。explorer_panel_width と同じ範囲に切り詰めた画素を DIP へ直し（切り捨て）、
// explorer_width_clamped で収める（設定の値として常に読める）。
[[nodiscard]] std::int32_t explorer_dragged_width(std::int32_t client_width, std::uint32_t dpi,
                                                  std::int32_t panel_pixels) noexcept;

// 面と本文の境の当たり（左右 3 DIP・1 画素以上・縦は面の帯）。面が消えているか幅が 0 なら偽。
// 面の行の当たり（panel の中）より先に引く。
[[nodiscard]] bool explorer_divider_hit(const ExplorerLayout &layout, std::uint32_t dpi,
                                        std::int32_t x, std::int32_t y) noexcept;

// 設定 explorer_width の値（10 進・先頭の 0 なし・explorer_width_minimum_dips〜
// explorer_width_limit_dips）。空と形の違う値は値なし（既定の式・Issue #1380 の設計席の決め・ADR
// 0170 の追補）。
[[nodiscard]] std::optional<std::int32_t> explorer_width_of(std::string_view text) noexcept;

// 面の高さ（本文の帯の全高）に入る行数（先頭のルート名の行を含む・切り捨て・#1125）。ルート名の行
// （root_height）が入らなければ 0、入れば 1 + 残りの高さ / line_height。面が出ているかに依らず、
// 本文の分割（ADR 0148）にも本文の字の大きさにも依らない（#1390）。
[[nodiscard]] std::size_t explorer_panel_lines(LayoutRect client, std::uint32_t dpi) noexcept;

// 面の index 行目の矩形（0 はルート名の行で高さ root_height、1 からは木の行で高さ line_height）。
// 横は面の左端から境界の手前まで。面が消えていれば幅 0 の矩形。描画と当たりが同じ 1 本を読む。
[[nodiscard]] LayoutRect explorer_line(const ExplorerLayout &layout, std::size_t index) noexcept;

// 点の下の面の行（0 はルート名の行）。面の外なら値なし。行の数の上限は見ない（呼び手が見えている
// 行の数と比べる）。explorer_line の逆。
[[nodiscard]] std::optional<std::size_t> explorer_line_at(const ExplorerLayout &layout,
                                                          std::int32_t x, std::int32_t y) noexcept;

// ルート名の欄（行の左右 root_padding の内側）。
[[nodiscard]] LayoutRect explorer_root_name(const ExplorerLayout &layout,
                                            const LayoutRect &line) noexcept;

// 木の行の部品（行 line・深さ depth・未保存か unsaved）。丸い面は行の左右 inset の内側、その中の
// 左の余白 + 深さ × 字下げに山形の升、続けて印の升、gap を空けて名前（右は余白の手前まで）。
// 未保存なら丸い面の右端から dot_inset の所を右端に径 dot の丸を置き、名前は丸の gap 手前で切る。
// どの矩形も丸い面の外へ出ない（升と丸の縦は行の中央）。
[[nodiscard]] ExplorerRowParts explorer_row_parts(const ExplorerLayout &layout,
                                                  const LayoutRect &line, std::size_t depth,
                                                  bool unsaved) noexcept;

// 深さ level の縦の線（1 DIP・border の太さ）の左端の x。深さ level の行の山形の升の中央で、
// 深さが level より深い行（開いたフォルダの子）に通す。
[[nodiscard]] std::int32_t explorer_guide_x(const ExplorerLayout &layout, const LayoutRect &line,
                                            std::size_t level) noexcept;
} // namespace nenenib::core
