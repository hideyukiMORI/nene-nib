#pragma once

#include "TabKey.hpp"

#include <windows.h>

#include <optional>

namespace nenenib::ui::win32
{
// Ctrl と組んだ仮想キーをタブの鍵へ写す（ADR 0056 の決定 10）。何が起きるかは
// core::tab_command_for が決める。Ctrl は呼び出し側で確認済みで、Alt のときは呼ばない。
// Shift と組むのは Tab だけ。Ctrl+Shift+T / F4 / W は今までどおり後ろの表へ流れる。
[[nodiscard]] inline std::optional<core::TabKey> tab_shortcut(WPARAM key, bool shift)
{
    // OS の仮想キーは開いた集合（CPP-017）。
    switch (key)
    {
    case 'T':
        return shift ? std::nullopt : std::optional{core::TabKey::control_t};
    case VK_TAB:
        return shift ? core::TabKey::control_shift_tab : core::TabKey::control_tab;
    case VK_F4:
        return shift ? std::nullopt : std::optional{core::TabKey::control_f4};
    case 'W':
        return shift ? std::nullopt : std::optional{core::TabKey::control_w};
    default:
        return std::nullopt;
    }
}
} // namespace nenenib::ui::win32
