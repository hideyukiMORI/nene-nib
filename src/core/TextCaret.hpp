#pragma once

#include "CaretAffinity.hpp"
#include "TextPosition.hpp"

namespace nenenib::core
{
struct TextCaret
{
    TextPosition position;
    CaretAffinity affinity = CaretAffinity::leading;
};
} // namespace nenenib::core
