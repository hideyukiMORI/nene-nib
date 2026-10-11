#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::core
{
// 折り返しを加味した窓の論理行範囲。belowは本文末尾の次を許す。
struct VimScreenWindow
{
    LineNumber first;
    LineNumber last;
    LineNumber below;
    LineNumber middle;
    std::size_t occupied;
};
} // namespace nenenib::core
