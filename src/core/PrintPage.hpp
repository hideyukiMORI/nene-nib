#pragma once

#include <cstddef>

namespace nenenib::core
{
// 印刷の 1 ページに載せる行の範囲（ADR 0177 の決定 5）。行は紙の幅で折り返した後の画面行の通し
// 番号（0 始まり）で、first_row から row_count 行。空の文書の 1 ページは row_count が 0。
struct PrintPage
{
    std::size_t first_row;
    std::size_t row_count;
};

// 等値は非メンバで書く（CPP-003）。
[[nodiscard]] constexpr bool operator==(const PrintPage &left, const PrintPage &right) noexcept
{
    return left.first_row == right.first_row && left.row_count == right.row_count;
}
} // namespace nenenib::core
