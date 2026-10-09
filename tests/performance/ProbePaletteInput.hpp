#pragma once

#include "CommandChoice.hpp"

#include <string>
#include <vector>

namespace nenenib::tests::performance
{
struct ProbePaletteInput
{
    std::string query;
    std::vector<core::CommandChoice> entries;
};
} // namespace nenenib::tests::performance
