#pragma once

#include "VimKey.hpp"
#include "VimModeSet.hpp"

#include <vector>

namespace nenenib::core
{
// 表 KeyMap の 1 行（ADR 0176 の決定 2）。左辺は 1〜3 打鍵、右辺は 1 打鍵以上で、`<leader>`
// は作るときに展開してある。remap が偽なら右辺を表で引き直さない（`noremap`）。
struct KeyMapping
{
    VimModeSet modes;
    std::vector<VimKey> lhs;
    std::vector<VimKey> rhs;
    bool remap = true;
};
} // namespace nenenib::core
