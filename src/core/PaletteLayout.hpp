#pragma once

#include "LayoutRect.hpp"
#include "PaletteHintPlace.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace nenenib::core
{
// 面に並ぶ行の数の上限（ADR 0062 の決定 2）。配置の見えている行数と面の窓の行数（ADR 0150 の
// 決定 1）が読む。
inline constexpr std::size_t palette_row_limit = 8;
// 検索欄の左端（キャレットの棒）と記号の案内の左端の間に空ける最小の幅（#1640）。
inline constexpr std::int32_t palette_hint_gap_dips = 24;

struct PaletteLayout
{
    LayoutRect panel;
    LayoutRect input;
    LayoutRect rows;
    LayoutRect footer;
    std::int32_t row_height;
    std::size_t visible_rows;
};

[[nodiscard]] PaletteLayout palette_layout(std::int32_t width, std::int32_t height,
                                           std::uint32_t dpi, std::size_t choices) noexcept;
[[nodiscard]] LayoutRect palette_row(const PaletteLayout &layout,
                                     std::size_t visible_index) noexcept;
[[nodiscard]] std::optional<std::size_t> palette_hit(const PaletteLayout &layout, std::int32_t x,
                                                     std::int32_t y) noexcept;
// 窓の大きさで見えている行数（上限 palette_row_limit・ADR 0150 の決定 1）。候補の件数に依らない
// 値で、application が面の窓の行数として渡す。描く行数は palette_layout の visible_rows。
[[nodiscard]] std::size_t palette_window_rows(std::int32_t width, std::int32_t height,
                                              std::uint32_t dpi) noexcept;

// 行の右端の補足の欄（ADR 0060 の決定 9）。行の内側（左右 16 DIP）の右端から幅 112 DIP。
// 内側が 補足 112 + すき間 12 + 名前の最小 120 DIP より狭ければ幅 0（補足を出さない）。
[[nodiscard]] LayoutRect palette_row_note(const LayoutRect &row, std::uint32_t dpi) noexcept;
// 中身の幅 content（物理画素・F1 の一覧の鍵の枠は文字の幅 ＋ 左右の余白を ui が測る）が 112 DIP
// より広ければ、欄をその幅に広げる（#1658）。広げた欄 ＋ すき間 12 ＋ 名前の最小 120 DIP が内側に
// 入らなければ幅 0（中身を途中で切らずに出さない）。content が 112 DIP 以下なら 2 引数と同じ欄。
[[nodiscard]] LayoutRect palette_row_note(const LayoutRect &row, std::uint32_t dpi,
                                          std::int32_t content) noexcept;
// 名前と場所の欄。`noted` で補足の欄が幅を持てばその左 12 DIP まで、そうでなければ行の内側の全部。
[[nodiscard]] LayoutRect palette_row_label(const LayoutRect &row, std::uint32_t dpi,
                                           bool noted) noexcept;
// 右端の欄 note（palette_row_note の答え）の左 12 DIP までの名前と場所の欄。note が幅 0 なら行の
// 内側の全部（#1658）。
[[nodiscard]] LayoutRect palette_row_label(const LayoutRect &row, std::uint32_t dpi,
                                           const LayoutRect &note) noexcept;
// 全文検索の当たりの行の 2 段（ADR 0167 の決定 5・採用案 docs/design/2026-11-16-finder-look.md の
// 「当たりの行」）。どちらも行の内側（左右 16 DIP）の幅で、上の段（行の本文・12.5 DIP の字）は行の
// 上 3 DIP から高さ 19 DIP、下の段（相対パスと行・11 DIP の字）はその下から高さ 15 DIP（行の 40 DIP
// に収まる）。行が低ければ行の下端で切る。
[[nodiscard]] LayoutRect palette_hit_line(const LayoutRect &row, std::uint32_t dpi) noexcept;
[[nodiscard]] LayoutRect palette_hit_place(const LayoutRect &row, std::uint32_t dpi) noexcept;
// 全文検索の面の入力欄の右の進み具合の置き場（ADR 0167 の決定 5・V-1773 の L-1）。width は文の幅、
// typed は打った検索語（入力行のプロンプトを含む）の幅（どちらも物理画素・ui が描画と同じ書式で
// 測る）。欄は検索欄の右端に右寄せで幅 width。検索語の右端から palette_hint_gap_dips を空けて入らな
// ければ（長い検索語と重なるなら）幅 0 で、文を出さない（途中で切らない）。
[[nodiscard]] LayoutRect palette_progress_place(const PaletteLayout &layout, std::uint32_t dpi,
                                                std::int32_t width, std::int32_t typed) noexcept;
// 記号の案内の置き場（#1640）。widths の k 番目は組 k から最後の組までの文の幅（物理画素・描画と
// 同じ書式で ui が測る）。検索欄の左端から palette_hint_gap_dips を空けた残りに入る最も前の組から
// 出し、欄の幅は出す文の幅（右端は検索欄の右端）。組の途中では切らない（文の左が欄の外へ出ない）。
[[nodiscard]] PaletteHintPlace palette_input_hint(const PaletteLayout &layout, std::uint32_t dpi,
                                                  std::span<const std::int32_t> widths) noexcept;
} // namespace nenenib::core
