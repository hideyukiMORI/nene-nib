#pragma once

#include <cstdint>

namespace nenenib::core
{
enum class OrdinaryCharacterAction : std::uint8_t
{
    previous,
    next,
    containing,
    backspace,
    erase_forward
};
} // namespace nenenib::core
