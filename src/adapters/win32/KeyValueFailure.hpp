#pragma once

#include <cstdint>

namespace nenenib::adapters::win32
{
enum class KeyValueFailure : std::uint8_t
{
    malformed,
    duplicate
};
} // namespace nenenib::adapters::win32
