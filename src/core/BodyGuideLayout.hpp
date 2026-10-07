#pragma once

#include "GuideRowLayout.hpp"

#include <array>

namespace nenenib::core
{
struct BodyGuideLayout
{
    std::array<GuideRowLayout, 3> rows;
    LayoutRect note;
};
} // namespace nenenib::core
