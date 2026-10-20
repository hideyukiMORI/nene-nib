#pragma once

#include <cstdint>

namespace nenenib::core
{
// 2 枠のときの画面の側（ADR 0148 の決定 2・8）。first は左（左右の分割）か上（上下の分割）、
// second は右か下。操作中の枠がどちら側かを SplitState が持ち、pane_layouts は位置の順で返す。
// 値を足したら落ちた switch を直す（CPP-002）。
enum class PaneSide : std::uint8_t
{
    first,
    second
};
} // namespace nenenib::core
