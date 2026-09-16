#pragma once

#include "VimAction.hpp"
#include "VimMotion.hpp"

namespace nenenib::core
{
// 動作 → オペレータが取れる移動の表の 1 行。ここに無い動作の後ろでは d が打ち消される。
struct VimMotionBinding
{
    VimAction action;
    VimMotion motion;
};
} // namespace nenenib::core
