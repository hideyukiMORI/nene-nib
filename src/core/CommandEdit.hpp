#pragma once

#include <cstdint>

namespace nenenib::core
{
enum class CommandEdit : std::uint8_t
{
    left,
    right,
    home,
    end,
    backspace,
    erase,
    complete_next,
    complete_previous
};
} // namespace nenenib::core
