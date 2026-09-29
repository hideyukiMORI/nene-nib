#pragma once

#include "TabStep.hpp"

namespace nenenib::application
{
// Ctrl+Tab / Ctrl+Shift+Tab の 1 回（ADR 0058 の決定 3）。歩きを始め（まだなら）、使った順の列の
// 上の隣へ切り替える。歩いている間は列を入れ替えない（確定は SettleRecentTab）。
struct WalkRecentTab
{
    core::TabStep step;
};
} // namespace nenenib::application
