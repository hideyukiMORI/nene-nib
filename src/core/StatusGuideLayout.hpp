#pragma once

#include "GuideRowLayout.hpp"

#include <array>

namespace nenenib::core
{
struct StatusGuideLayout
{
    std::array<GuideRowLayout, 2> rows;
};
} // namespace nenenib::core
