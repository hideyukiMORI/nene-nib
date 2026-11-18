#pragma once

#include "MarkdownDocument.hpp"
#include "PreviewBlock.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace nenenib::core
{
// 表の行の升を切り出す純関数（#1848・D103）。表の行の要素（table_row）は升の文字を Tab でつないだ
// 1 つの text と装飾を持ち（コピーはそのまま升の間が Tab）、描く所と位置の解決は升ごとに文字組み
// を作る。

// row の column 番目の升の、row の text の中の範囲 [first, second)（升の間の Tab は含まない）。
[[nodiscard]] std::pair<std::size_t, std::size_t> preview_table_cell_range(const PreviewBlock &row,
                                                                           std::size_t column);

// row の column 番目の升の要素。種類は table_row のまま（保持の鍵が本文の要素と混ざらない）、
// 文字は升の文字、装飾は升の中へ写し、見出しの行は升の全体に strong を先頭に足す。深さ・記号・
// 位置・升は持たない（升の文字組みの鍵を行の位置に依らせない）。
[[nodiscard]] PreviewBlock preview_table_cell(const PreviewBlock &row, std::size_t column);

// index の要素（table_row）が属する表の行の範囲 [first, second)（文書の要素の番号）。表の最初の行は
// 自分の番号から行の番号を引いた位置で、終わりは行の番号が 1 ずつ続く所まで。
[[nodiscard]] std::pair<std::size_t, std::size_t>
preview_table_rows(const MarkdownDocument &document, std::size_t index);
// 表の行 rows の升の文字・装飾・見出しかの要約（FNV-1a 64・位置は入れない）。表を閉じるときに
// 1 回だけ作り、列の幅の覚えが表の同一性を行の数に依らず 1 回で決める（#1852 のレビュー）。
[[nodiscard]] std::uint64_t preview_table_digest(std::span<const PreviewBlock> rows) noexcept;
} // namespace nenenib::core
