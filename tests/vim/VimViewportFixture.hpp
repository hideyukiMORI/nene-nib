#pragma once

#include <cstdint>

namespace nenenib::tests
{
struct VimViewportFixture
{
    std::uint32_t visible_lines;
    std::uint32_t first_visible;
    std::uint32_t line;
    std::uint32_t column;
    std::uint32_t expected_first_visible;
    std::uint32_t expected_scroll_lines;
    // 0は旧fixtureの折り返しなし。正値は決定的な文字組み（1文字10px）の幅。
    std::uint32_t wrap_columns = 0;
    std::uint32_t expected_first_row = 0;
    // 0は折り返す/旧fixture。正値は折り返さない窓の升の数（1升10px・ADR 0145）と、鍵の前と後の
    // leftcol（winsaveview().leftcol）。
    std::uint32_t nowrap_columns = 0;
    std::uint32_t left_column = 0;
    std::uint32_t expected_left_column = 0;
    // 折り返す窓で、鍵の前に first_visible の行の上から隠れている画面行の数（skipcol / 幅・
    // ADR 0146 の決定 5）。
    std::uint32_t first_row = 0;
};
} // namespace nenenib::tests
