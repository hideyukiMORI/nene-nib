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
};
} // namespace nenenib::tests
