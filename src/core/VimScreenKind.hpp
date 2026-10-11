#pragma once

#include <cstdint>

namespace nenenib::core
{
enum class VimScreenKind : std::uint8_t
{
    top,
    middle,
    bottom,
    half_up,
    half_down,
    page_up,
    page_down
};
} // namespace nenenib::core
