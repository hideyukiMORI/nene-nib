#pragma once

#include <cstdint>

namespace nenenib::core
{
// 文字ではない鍵の閉じた一覧（ADR 0012 の決定 4）。窓は WM_KEYDOWN をこの値に写すだけ。
enum class VimSpecialKey : std::uint8_t
{
    escape,
    enter,
    backspace,
    arrow_left,
    arrow_right,
    arrow_up,
    arrow_down,
    control_r
};
} // namespace nenenib::core
