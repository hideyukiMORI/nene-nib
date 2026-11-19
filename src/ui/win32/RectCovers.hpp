#pragma once

#include <cstdint>
#include <d2d1.h>

namespace nenenib::ui::win32
{
// 描いた箱 box（物理画素・左と上を含み右と下を含まない）が点 (x, y) を覆うか（hover の札の当たり・
// #1884 の再確認 M-a）。
[[nodiscard]] inline bool rect_covers(const D2D1_RECT_F &box, std::int32_t x,
                                      std::int32_t y) noexcept
{
    const auto px = static_cast<float>(x);
    const auto py = static_cast<float>(y);
    return px >= box.left && px < box.right && py >= box.top && py < box.bottom;
}
} // namespace nenenib::ui::win32
