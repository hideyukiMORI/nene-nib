#pragma once

#include "VimAction.hpp"
#include "VimActionGroup.hpp"

namespace nenenib::core
{
// 動作 → 大分類の表の 1 行（CPP-012 / ADR 0006・T8）。行が無い動作は何もしない。
struct VimActionBinding
{
    VimAction action;
    VimActionGroup group;
};
} // namespace nenenib::core
