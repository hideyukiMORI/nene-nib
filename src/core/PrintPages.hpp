#pragma once

#include "PrintPage.hpp"
#include "PrintPalette.hpp"
#include "PrintSheet.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nenenib::core
{
// 印刷の余白（上下左右・固定・ADR 0177 の決定 5）。
inline constexpr std::uint32_t print_margin_millimetres = 20;

// 印刷を頼む本文の上限（ADR 0177 の決定 7 と追補・印刷の header の定数）。これを超える本文は port
// を呼ばず「印刷は 16 MiB までです」を出す。ページの数には上限を置かない（OS のスプールに任せる）。
inline constexpr std::size_t print_byte_limit = std::size_t{16} * 1024 * 1024;

// 余白 20 mm を dpi の論理画素に直す（整数演算・四捨五入）。
[[nodiscard]] std::uint32_t print_margin(std::uint32_t dpi) noexcept;

// 1 ページの本文の行数（決定
// 5）。紙の高さから上下の余白とヘッダを引き、行の高さで割る（切り捨て）。 行が 1
// つも入らない紙（字が紙より大きい）でも 1 行は載せる（ページの列が終わるように）。
[[nodiscard]] std::uint32_t print_rows_per_page(const PrintSheet &sheet) noexcept;

// 画面行の数 rows を 1 ページ rows_per_page 行ずつに割ったページの列。行は紙の幅で折り返した後の
// 画面行（折り返しは本文と同じ DirectWrite の書式で ui が決める）。空の文書は行の無い 1 ページ。
// rows_per_page が 0 なら 1 として扱う。
[[nodiscard]] std::vector<PrintPage> print_pages(std::size_t rows, std::uint32_t rows_per_page);

// 印刷の配色（決定 4）。light の既定テーマ（theme_for(Appearance::light)）の Palette と構文の表で、
// 地は白・字は黒に置き換える。選択・キャレット・検索の面・IME の色は印刷では使わない。
[[nodiscard]] PrintPalette print_palette() noexcept;

// ヘッダの右のページ番号「n / N」（決定 5）。number は 1 始まり。
[[nodiscard]] std::string print_page_label(std::size_t number, std::size_t count);
} // namespace nenenib::core
