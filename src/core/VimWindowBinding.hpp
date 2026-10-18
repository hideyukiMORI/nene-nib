#pragma once

#include "VimWindowCommand.hpp"

namespace nenenib::core
{
// `Ctrl-w` の後ろの鍵 → 窓の命令の表の 1 行（CPP-012・ADR 0148 の決定 7）。
struct VimWindowBinding
{
    char32_t key;
    VimWindowCommand command;
};
} // namespace nenenib::core
