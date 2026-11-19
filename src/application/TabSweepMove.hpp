#pragma once

#include <cstdint>

namespace nenenib::application
{
// まとめて閉じる道の次の一歩（ADR 0078 の追補 #1790 の決定 6）。close はアクティブでない相手を
// 1 つ名指しで閉じる（切り替えない・読まない）、switch_to_keep は相手がアクティブだけになったので
// 残すタブへ切り替える、done は閉じる相手が無い。
enum class TabSweepMove : std::uint8_t
{
    close,
    switch_to_keep,
    done
};
} // namespace nenenib::application
