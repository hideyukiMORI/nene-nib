#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 描画用の 1 行（ADR 0040 の決定 1）。本文の行から `display_line` が作る純粋な値で、制御文字
// （表で `wide` かつ U+0020 未満）は `^X` の 2 文字、書式用文字（表で `unprintable`）は `<xxxx>`
// の 6 文字に置き換えてある。表は ADR 0034 の `display_width` の 1 本だけを見る。
//
// `starts[i]` は本文の i 番目の code point が始まる描画の code point 位置。大きさは本文の
// code point 数 + 1 で、末尾は描画の code point 数である。
struct DisplayLine
{
    std::string text;
    std::vector<std::size_t> starts;
};

// 本文の 1 行（`\n` を含まない検証済みの UTF-8）から描画用の行を作る。
[[nodiscard]] DisplayLine display_line(std::string_view text);

// 本文の桁 → 描画の位置。`column` が範囲外なら描画の末尾。
[[nodiscard]] std::size_t display_position(const DisplayLine &line, std::size_t column) noexcept;

// 描画の位置 → 本文の桁。置き換えの途中の位置はその文字の桁、末尾を超えれば本文の末尾。
[[nodiscard]] std::size_t source_column(const DisplayLine &line, std::size_t position) noexcept;

// 本文の桁 `column` の文字が置き換えられたか（描画で 2 文字以上を占めるか）。範囲外は false。
[[nodiscard]] bool is_replaced(const DisplayLine &line, std::size_t column) noexcept;
} // namespace nenenib::core
