#pragma once

#include "EasyMotionKind.hpp"
#include "Offset.hpp"

#include <string>
#include <vector>

namespace nenenib::core
{
// 印が出ているあいだの命令の途中の値（ADR 0163 の決定 4）。targets はキャレットから近い順の跳び先
// （easymotion_targets）、labels は同じ位置の印（easymotion_labels・どの印も他の印の接頭辞に
// ならない）、typed はここまでに打った印の文字（ASCII）。VimState.easy_motion
// に置き、vim_resting_from で消える側。値は単独で妥当な 公開 aggregate で、絞るのは easymotion_step
// の 1 本。
struct EasyMotionState
{
    EasyMotionKind kind;
    std::vector<Offset> targets;
    std::vector<std::string> labels;
    std::string typed;
};
} // namespace nenenib::core
