#pragma once

#include "CommandChoice.hpp"

#include <cstddef>

namespace nenenib::core
{
struct CommandMatch
{
    CommandChoice choice;
    std::size_t score;
};
} // namespace nenenib::core
