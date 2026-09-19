#pragma once

#include <cstdint>

namespace nenenib::core
{
enum class CommandChoiceKind : std::uint8_t
{
    fill,
    execute
};
} // namespace nenenib::core
