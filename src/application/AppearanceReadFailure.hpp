#pragma once

#include <cstdint>

namespace nenenib::application
{
enum class AppearanceReadFailure : std::uint8_t
{
    unavailable,
    unreadable
};
} // namespace nenenib::application
