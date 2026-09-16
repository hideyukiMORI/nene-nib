#pragma once

#include "VimKey.hpp"

namespace nenenib::application
{
// Vim モードで打たれた鍵 1 つ（ADR 0012 の決定 4）。窓は WM_CHAR / WM_KEYDOWN をこの値に
// 写すだけで、何が起きるかは core の vim_step が決める（ARC-011）。
struct VimKeyPress
{
    core::VimKey key;
};
} // namespace nenenib::application
