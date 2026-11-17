#pragma once

#include "Offset.hpp"

namespace nenenib::core
{
// 印の鍵で印が 1 つに決まったとき（ADR 0163 の決定 5）。target はその印の跳び先。
struct EasyMotionJump
{
    Offset target;
};
} // namespace nenenib::core
