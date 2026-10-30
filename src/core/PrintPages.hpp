#pragma once

#include "PrintPage.hpp"
#include "PrintSheet.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nenenib::core
{
// 印刷の余白（上下左右・固定・ADR 0177 の決定 5）。
inline constexpr std::uint32_t print_margin_millimetres = 20;

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

// ヘッダの右のページ番号「n / N」（決定 5）。number は 1 始まり。
[[nodiscard]] std::string print_page_label(std::size_t number, std::size_t count);
} // namespace nenenib::core
