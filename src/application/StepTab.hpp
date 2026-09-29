#pragma once

#include "TabStep.hpp"

namespace nenenib::application
{
// 隣のタブへ切り替える（ADR 0056 の決定 3）。帯の位置の順で、端は折り返す。
struct StepTab
{
    core::TabStep step;
};
} // namespace nenenib::application
