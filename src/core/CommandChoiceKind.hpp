#pragma once

#include <cstdint>

namespace nenenib::core
{
enum class CommandChoiceKind : std::uint8_t
{
    fill,
    execute,
    open
};
} // namespace nenenib::core
