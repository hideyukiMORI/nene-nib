#pragma once

#include <cstdint>

namespace nenenib::core
{
enum class ThemeNameFailure : std::uint8_t
{
    invalid,
    too_long
};
} // namespace nenenib::core
