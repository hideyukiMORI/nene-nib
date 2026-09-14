#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
enum class RenderFailure : std::uint8_t
{
    device_creation,
    swap_chain,
    composition,
    direct2d,
    directwrite,
    device_lost
};
} // namespace nenenib::ui::win32
