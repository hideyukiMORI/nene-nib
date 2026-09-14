#pragma once

#include "CaretMotion.hpp"
#include "SelectionAnchoring.hpp"

namespace nenenib::application
{
// キャレットの移動。Shift の有無は anchoring で表す（ADR 0009 の決定 4）。
struct MoveCaret
{
    core::CaretMotion motion;
    core::SelectionAnchoring anchoring;
};
} // namespace nenenib::application
