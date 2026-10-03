#pragma once

#include "CaretMotion.hpp"
#include "CaretUnit.hpp"

#include <cstddef>

namespace nenenib::core
{
struct CaretMoveRequest
{
    CaretMotion motion;
    std::size_t page_lines;
    CaretUnit unit;
};
} // namespace nenenib::core
