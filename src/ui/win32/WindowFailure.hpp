#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
enum class WindowFailure : std::uint8_t
{
    class_registration,
    creation,
    render
};
} // namespace nenenib::ui::win32
