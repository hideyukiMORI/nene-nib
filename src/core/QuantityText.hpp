#pragma once

#include <cstddef>
#include <string>

namespace nenenib::core
{
// 文に差し込む固定の数の文字列（ADR 0123 の追補 2026-11-13 の決定 5・#1019）。定数と文を手で
// 揃えないため、文の置き場には定数からここで作った文字列を渡す。

// 2 の冪の単位の大きさ（`64 MiB` `1 GiB` `16 KiB`）。割り切れる一番大きい単位で、どれでも割り
// 切れなければ `<数> bytes`。
[[nodiscard]] std::string binary_size_text(std::size_t bytes);

// 3 桁ごとに `,` で区切った 10 進（`100,000`・今の日本語の文の形）。
[[nodiscard]] std::string grouped_decimal_text(std::size_t value);
} // namespace nenenib::core
