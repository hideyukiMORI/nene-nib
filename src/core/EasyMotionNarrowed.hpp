#pragma once

#include "EasyMotionState.hpp"

namespace nenenib::core
{
// 印の鍵がまだ印を 1 つに決めない接頭辞だったとき（ADR 0163 の決定 5）。state は typed を
// 伸ばした値で、跳び先と印は変えない（出す印は typed で始まるものだけ）。
struct EasyMotionNarrowed
{
    EasyMotionState state;
};
} // namespace nenenib::core
