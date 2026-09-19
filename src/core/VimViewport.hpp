#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::core
{
// 1 回の Vim の鍵が読む表示領域。所有者は application の ScrollState であり、これは
// engine に渡す値だけを写した入力（ADR 0019 の決定 1）。
struct VimViewport
{
    LineNumber first_visible;
    std::size_t visible_lines;
};
} // namespace nenenib::core
