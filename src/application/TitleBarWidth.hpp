#pragma once

#include <cstdint>

namespace nenenib::application
{
// 帯の幅（DIP）を知らせる（ADR 0056 の決定 3）。`VisibleLines` と同じく窓の寸法を 1 意図で送り、
// application は送り量をアクティブなタブが見える所へ直す。
struct TitleBarWidth
{
    std::int32_t dip;
};
} // namespace nenenib::application
