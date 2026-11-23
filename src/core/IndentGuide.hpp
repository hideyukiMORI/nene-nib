#pragma once

#include "IndentGuidePaint.hpp"
#include "IndentGuideRun.hpp"
#include "IndentGuideStyle.hpp"
#include "IndentUnit.hpp"
#include "LineNumber.hpp"
#include "Palette.hpp"
#include "RainbowColors.hpp"
#include "RgbColor.hpp"
#include "SyntaxPalette.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace nenenib::core
{
class TextBuffer;

// インデントガイドの純関数（ADR 0193 の決定 5・7・9・#1975）。字下げの数え・段の幅の推し当て・
// 空の行の続き・強める段の区間・虹の色はここの 1 本ずつで、application と renderer は呼ぶだけ。

// 行頭の字下げの仮想桁（空白は 1 桁・Tab は次の vim_tab_stop の倍数まで・全角空白 U+3000
// は数えない・ Vim の get_indent_str と同じ規則・決定 7）。line
// は行頭からの文字列で、字下げの後ろは読まない。
[[nodiscard]] std::size_t indent_cells(std::string_view line) noexcept;
// 行頭の空白と Tab の列のバイト数。
[[nodiscard]] std::size_t indent_bytes(std::string_view line) noexcept;
// 空の行か（空・空白と Tab だけ・CRLF の行の終わりの `\r` も空白と同じに読む）。
[[nodiscard]] bool blank_indent_line(std::string_view line) noexcept;
// 本文の行 line の字下げの桁。空の行は値なし。行を写さず、行頭の窓だけを読む（巨大な行でも先頭の
// 字下げの分だけ）。
[[nodiscard]] std::optional<std::size_t> line_indent_cells(const TextBuffer &text, LineNumber line);

// 推し当てが見る本文の先頭（決定 7）。行の数と、バイト数（窓の端は whole_code_points で切る）。
inline constexpr std::size_t indent_guess_lines = 1000;
inline constexpr std::size_t indent_guess_bytes = std::size_t{64} * 1024;
// 段の幅の推し当て（決定 7）。head は本文の先頭（行の数は indent_guess_lines で打ち切る）。空でない
// 行を順に見て、Tab で始まる行は tab に 1 票、空白で始まる行が並ぶ所（一方が空白で始まり、どちらも
// Tab で始まらない隣り合う空でない行・間の空の行は飛ばす）は字下げの差が 2 / 4 / 8 なら その差に
// 1 票（差 6 などほかの差は票にしない）。tab の票が空白の票の和より多ければ tab、そうでなければ
// 最も票の多い差（同数は小さい方）。票の和が 3 未満なら値なし。
[[nodiscard]] std::optional<IndentUnit> guessed_indent_unit(std::string_view head) noexcept;
// 本文の先頭 indent_guess_bytes を読んで推し当てる（開く道の 1 か所が呼ぶ・決定 7）。
[[nodiscard]] std::optional<IndentUnit> guessed_indent_unit(const TextBuffer &text);
// 段の幅の桁（tab は vim_tab_stop）。
[[nodiscard]] std::size_t indent_unit_cells(IndentUnit unit) noexcept;

// 設定の段の幅（settings.v6 の `indent_guide_width`・`:set indentguidewidth=`）。2 / 4 / 8 だけ。
[[nodiscard]] std::optional<std::uint8_t> indent_guide_width_of(std::string_view text) noexcept;
// 設定の段の幅の段（2 → 4 → 8・端で止まる）。
[[nodiscard]] std::uint8_t indent_guide_width_stepped(std::uint8_t width, bool forward) noexcept;
// 文書の推し当てと設定から、ガイドに使う段の幅の桁。
[[nodiscard]] std::size_t indent_guide_step(std::optional<IndentUnit> guessed,
                                            std::uint8_t width) noexcept;

// 虹の色の数（settings.v6 の `indent_guide_colors`・3〜6）。
inline constexpr std::uint8_t indent_guide_colors_minimum = 3;
inline constexpr std::uint8_t indent_guide_colors_maximum = 6;
[[nodiscard]] std::optional<std::uint8_t> indent_guide_colors_of(std::string_view text) noexcept;
[[nodiscard]] std::uint8_t indent_guide_colors_stepped(std::uint8_t count, bool forward) noexcept;

// 単色 / 虹の名前（`rainbow` `plain`）とその逆。
[[nodiscard]] std::string_view indent_guide_style_name(IndentGuideStyle style) noexcept;
[[nodiscard]] std::optional<IndentGuideStyle>
indent_guide_style_named(std::string_view name) noexcept;

// 空の行の続き（決定 9）。cells は見えている行ごとの字下げ（空の行は値なし）、before / after は
// 見えている範囲の外の上と下の最も近い空でない行の字下げ（呼び手が 1
// 画面まで探す・無ければ値なし）。
// 空の行は上と下の空でない行の字下げの大きい方、片方だけ届けばその値、どちらも無ければ 0。
[[nodiscard]] std::vector<std::size_t>
continued_indents(std::span<const std::optional<std::size_t>> cells,
                  std::optional<std::size_t> before, std::optional<std::size_t> after);

// 強める段の区間（決定 9）。levels は見えている行ごとの段の数（続きを済ませた値）、caret
// はキャレット の行の位置。キャレットの行の段 L が 0 なら値なし。そうでなければ段 L − 1
// の線を、キャレットの行 から上下に段が L 以上の行が続く区間（列の中で打ち切る）で強める。
[[nodiscard]] std::optional<IndentGuideRun> active_indent_run(std::span<const std::size_t> levels,
                                                              std::size_t caret) noexcept;

// 虹の色（決定 5）。SyntaxPalette の役割の色を keyword function string type number operators
// constant warning error の順に見て、取った色と互いに ΔE76 ≥ 20・本文の色（foreground）から ≥ 15 の
// ものを貪欲に取る（最大 6 色）。count は min(取れた数, wanted)。palette は ADR の署名のまま受け、
// いまは読まない（役割の色が背景と見分けられることは ThemeContrast の試験の前提）。
[[nodiscard]] RainbowColors rainbow_colors(const SyntaxPalette &syntax, const Palette &palette,
                                           std::size_t wanted) noexcept;

// 強めない段の線を背景へ寄せる割合（%・決定 5 の「役割の色 45%・背景 55%」・hide の試用で変える
// ときはこの 1 か所）。
inline constexpr std::uint8_t indent_guide_faded_percent = 55;

// 1 frame の線の色の材料（決定 5）。wanted は虹の色の数（設定の indent_guide_colors）。
[[nodiscard]] IndentGuidePaint indent_guide_paint(IndentGuideStyle style, std::size_t wanted,
                                                  const SyntaxPalette &syntax,
                                                  const Palette &palette) noexcept;
// 段 level の線の色。strong は強める段（虹は役割の色そのもの・単色は muted）。強めない段は虹なら
// 役割の色を背景へ indent_guide_faded_percent だけ寄せた色、単色なら panel_border。虹の色が 1 つも
// 取れないテーマは単色と同じ。
[[nodiscard]] RgbColor indent_guide_color(const IndentGuidePaint &paint, std::size_t level,
                                          bool strong) noexcept;
} // namespace nenenib::core
