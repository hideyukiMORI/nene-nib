#pragma once

#include "EditorSettings.hpp"
#include "InvisibleKind.hpp"
#include "InvisibleSpan.hpp"
#include "LineBreakMark.hpp"
#include "LineNumber.hpp"
#include "ListSpace.hpp"
#include "Palette.hpp"
#include "RgbColor.hpp"
#include "TextBuffer.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

// 不可視文字の区間と印の色（ADR 0193 の決定 1・4・5・10・#1977・#1011 の行末の空白）。
namespace nenenib::core
{
// 空白の印の出し方の名前（`none` `trail` `all`・settings.v7 の `list_space` と `:set listspace=`
// の値）とその逆。表に無い名前は値なし。
[[nodiscard]] std::string_view list_space_name(ListSpace space) noexcept;
[[nodiscard]] std::optional<ListSpace> list_space_named(std::string_view name) noexcept;

// 1 行の本文（`\n` を含まない検証済みの UTF-8・LineView::text）を 1 回歩いた区間（決定 10）。種類は
// 設定の list_space・list_tab・list_wide_space で選ぶ（全体の `features=list` は呼び手が見る）。
// 空白（U+0020）は list_space が trail なら行末の空白の列だけ（`trailing_space`）、all
// なら全部（行末の 分は trailing_space・ほかは space）。行末の空白の列は、最後の「空白でも Tab
// でもない字」の後ろ （Vim の trail と同じく Tab を挟んでも続く）。Tab は list_tab
// が真のとき、全角空白（U+3000）は list_wide_space が真のとき。同じ種類の続きは 1
// つの区間にまとめる。行の外を見ない。
[[nodiscard]] std::vector<InvisibleSpan> invisible_spans(std::string_view line,
                                                         const EditorSettings &settings);

// 行 line の改行の形（決定 10）。行の内容の終わりから次の行の先頭までのバイト（text_range）が
// `\r\n` なら crlf、`\n` なら lf、無ければ（改行の無い最後の行）値なし。LF の本文の `\r\n` の行は
// `\r` が行の内容（`^M` の置き換えで見えている）なので lf。読むのは行の終わりの 1〜2 バイトだけ。
[[nodiscard]] std::optional<LineBreakMark> line_break_mark(const TextBuffer &text, LineNumber line);

// 印の色（決定 5）。行末の空白と全角空白は強めの色（accent を背景へ
// invisible_strong_faded_percent だけ寄せた色）、ほかと改行の印は muted を背景へ
// invisible_faded_percent だけ寄せた色（見本 A の不透明度 0.9 / 0.75 に当たる・試用で変えるときは
// この 2 つ）。色の対応はこの関数だけが持つ（ui に色のリテラルを書かない・ADR 0008 決定 8）。
inline constexpr std::uint8_t invisible_faded_percent = 25;
inline constexpr std::uint8_t invisible_strong_faded_percent = 10;
[[nodiscard]] RgbColor invisible_mark_color(const Palette &palette, InvisibleKind kind) noexcept;
[[nodiscard]] RgbColor line_break_mark_color(const Palette &palette) noexcept;
} // namespace nenenib::core
