#pragma once

#include <cstddef>

namespace nenenib::application
{
// 帯の位置 index（0 始まり）のタブへ切り替える（ADR 0056 の決定 3）。範囲の外なら何もしない。
struct SwitchTab
{
    std::size_t index;
};
} // namespace nenenib::application
