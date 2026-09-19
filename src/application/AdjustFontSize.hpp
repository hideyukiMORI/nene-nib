#pragma once

#include "FontSizeAdjustment.hpp"

#include <cstddef>

namespace nenenib::application
{
struct AdjustFontSize
{
    core::FontSizeAdjustment adjustment;
    std::size_t steps;
};
} // namespace nenenib::application
