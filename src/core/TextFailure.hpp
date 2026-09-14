#pragma once

#include <cstdint>

namespace nenenib::core
{
enum class TextFailure : std::uint8_t
{
    empty,
    control_character,
    invalid_utf8,
    too_long
};
} // namespace nenenib::core
