#pragma once

#include "VimAction.hpp"

namespace nenenib::core
{
// NORMAL の鍵 → 動作の表の 1 行（CPP-012 / ADR 0006・T8）。分岐で書くと関数長で落ちる。
struct VimBinding
{
    char32_t key;
    VimAction action;
};
} // namespace nenenib::core
