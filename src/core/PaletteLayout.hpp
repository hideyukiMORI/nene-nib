#pragma once

#include "FinderPreview.hpp"
#include "LayoutRect.hpp"
#include "PaletteContent.hpp"
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

// 面の配置（ADR 0062・ADR 0189 の決定 1）。描画と当たりの判定は同じ値を読む。
struct PaletteLayout
{
    LayoutRect panel;
    LayoutRect input;
    LayoutRect rows;
    LayoutRect footer;
    std::int32_t row_height;
    std::size_t visible_rows;
    // 描く置き方（right / below / off・automatic にはならない）。
    FinderPreview preview = FinderPreview::off;
    // プレビュー欄の全体・見出し（高さ 28 DIP）・本文・見出しの右端の切り替えの印（28 DIP 四方）。
    // off ではどれも幅 0。
    LayoutRect pane = {};
    LayoutRect pane_header = {};
    LayoutRect pane_body = {};
    LayoutRect pane_toggle = {};
};

// 置き方 placement（automatic を含む・欄を持たない出どころは呼び出し元が off にする・
// palette_placement）と窓の大きさから面の配置を決める 1 本（ADR 0189 の決定 1）。
// - off: 今の面のまま（幅 640 DIP・行は候補の数だけ最大 8）。
// - right: 幅 min(1000 DIP, 窓の幅 − 32 DIP)・左の当たりの欄 400 DIP・その右がプレビュー欄（高さは
//   行の欄と同じ）。行は窓に入るだけ最大 8（候補の数に依らない）。
// - below: 幅 640 DIP・当たり最大 5 行・その下にプレビュー欄（見出し 28 DIP ＋ 残りの高さ・最小 6
//   行ぶん・最大 40 行ぶん）。
// 入らない窓では palette_preview_drawn の形に落とす（覚えた値は変えない）。
[[nodiscard]] PaletteLayout palette_layout(std::int32_t width, std::int32_t height,
                                           std::uint32_t dpi, PaletteContent content) noexcept;
// 置き方と窓の大きさから描く形（ADR 0189 の決定 1）。automatic と right は right が入れば right
// （プレビュー欄の幅 360 DIP 以上 = 窓の幅 792 DIP 以上）、入らなければ below（当たり 3 行 ＋
// プレビュー 6 行が窓の高さに入る）、それも入らなければ off。below は入れば below・入らなければ
// off。off は off。
[[nodiscard]] FinderPreview palette_preview_drawn(FinderPreview placement, std::int32_t width,
                                                  std::int32_t height, std::uint32_t dpi) noexcept;
// プレビュー欄の本文に入る行の数（最大 finder_preview_line_limit・行の高さ 19 DIP・上下 6 DIP）。
[[nodiscard]] std::size_t palette_preview_lines(const PaletteLayout &layout,
                                                std::uint32_t dpi) noexcept;
// 本文の index 行目の矩形（欄の幅・行の高さ 19 DIP）。
[[nodiscard]] LayoutRect palette_preview_line(const PaletteLayout &layout, std::uint32_t dpi,
                                              std::size_t index) noexcept;
// 行の中の行番号の欄（左から幅 40 DIP・右寄せで描く）と本文の欄（行番号の右 14 DIP から）。
[[nodiscard]] LayoutRect palette_preview_number(const LayoutRect &line, std::uint32_t dpi) noexcept;
[[nodiscard]] LayoutRect palette_preview_text(const LayoutRect &line, std::uint32_t dpi) noexcept;
// 見出しの文の欄（左右 14 DIP の内側・右端は切り替えの印の手前）。
[[nodiscard]] LayoutRect palette_preview_heading(const PaletteLayout &layout,
                                                 std::uint32_t dpi) noexcept;
// 点が見出しの右端の切り替えの印の上か（押すと Ctrl+Shift+P と同じ・決定 3）。
[[nodiscard]] bool palette_toggle_hit(const PaletteLayout &layout, std::int32_t x,
                                      std::int32_t y) noexcept;
[[nodiscard]] LayoutRect palette_row(const PaletteLayout &layout,
                                     std::size_t visible_index) noexcept;
[[nodiscard]] std::optional<std::size_t> palette_hit(const PaletteLayout &layout, std::int32_t x,
                                                     std::int32_t y) noexcept;
// 窓の大きさと置き方で見えている行数（上限 palette_row_limit・ADR 0150 の決定 1・below は 5）。
// 候補の件数に依らない値で、application が面の窓の行数として渡す。描く行数は palette_layout の
// visible_rows。
[[nodiscard]] std::size_t palette_window_rows(std::int32_t width, std::int32_t height,
                                              std::uint32_t dpi, FinderPreview placement) noexcept;

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
