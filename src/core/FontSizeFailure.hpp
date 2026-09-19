#pragma once

#include <cstdint>

namespace nenenib::core
{
enum class FontSizeFailure : std::uint8_t
{
    out_of_range,
    invalid_text
};
} // namespace nenenib::core
