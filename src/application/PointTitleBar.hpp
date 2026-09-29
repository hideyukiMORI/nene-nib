#pragma once

#include "TitleBarTarget.hpp"

#include <optional>

namespace nenenib::application
{
// マウスを載せた帯の要素（ADR 0056 の決定 3・9）。変わったときだけ ui が送り、窓を出たら nullopt。
struct PointTitleBar
{
    std::optional<core::TitleBarTarget> target;
};
} // namespace nenenib::application
