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
};
} // namespace nenenib::tests
