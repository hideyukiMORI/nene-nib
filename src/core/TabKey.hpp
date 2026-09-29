#pragma once

#include <cstdint>

namespace nenenib::core
{
// タブの鍵の閉じた一覧（ADR 0056 の決定 10）。ui は OS の仮想キーをこれへ写すだけで、何が起きるかは
// tab_command_for が決める。値が増えたら写し先の足りない `switch` がコンパイルで落ちる（CPP-002）。
enum class TabKey : std::uint8_t
{
    control_t,
    control_tab,
    control_shift_tab,
    control_f4,
    control_w
};
} // namespace nenenib::core
