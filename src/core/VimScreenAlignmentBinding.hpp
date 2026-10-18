#pragma once

#include "VimAction.hpp"
#include "VimScreenAlignment.hpp"

namespace nenenib::core
{
// 動作 → 縦の位置合わせの意味の表の 1 行（ADR 0146 の決定 2・CPP-012）。
struct VimScreenAlignmentBinding
{
    VimAction action;
    VimScreenAlignment alignment;
};
} // namespace nenenib::core
