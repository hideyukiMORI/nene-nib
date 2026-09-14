#pragma once

#include <cstdint>

namespace nenenib::application
{
enum class EditorIntent : std::uint8_t
{
    refresh_appearance,
    select_ordinary_mode,
    select_vim_mode
};
} // namespace nenenib::application
