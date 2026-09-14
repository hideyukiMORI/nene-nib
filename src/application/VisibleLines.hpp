#pragma once

#include <cstddef>

namespace nenenib::application
{
// 窓の大きさが変わったときに UI が知らせる「本文が何行入るか」。行数の所有は application。
struct VisibleLines
{
    std::size_t lines;
};
} // namespace nenenib::application
