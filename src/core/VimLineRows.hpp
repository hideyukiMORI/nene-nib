#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::core
{
// 1打鍵の間だけ借りる文字組みの結果。本文やOS資源を所有しない。
struct VimLineRows
{
    LineNumber line;
    std::size_t rows;
};
} // namespace nenenib::core
