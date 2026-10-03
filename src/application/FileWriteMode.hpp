#pragma once

#include <cstdint>

namespace nenenib::application
{
enum class FileWriteMode : std::uint8_t
{
    replace,
    create_new
};
} // namespace nenenib::application
