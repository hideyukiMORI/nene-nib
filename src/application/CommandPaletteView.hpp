#pragma once

#include "CommandChoice.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::application
{
struct CommandPaletteView
{
    std::vector<core::CommandChoice> choices;
    std::size_t selected;
};
} // namespace nenenib::application
