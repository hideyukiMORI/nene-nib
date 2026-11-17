#pragma once

#include <windows.h>

namespace nenenib::ui::win32
{
// WM_CONTEXTMENU が鍵（Shift+F10・メニューキー）から来たか（lParam が -1・#1051）。マウスから来た分
// （帯の空きの右クリック）は画面の座標で、窓は開かずに既定の処理（窓のメニュー）へ渡す。窓を作らずに
// 試せるようここに置く（nib_window_tests の `--standard-keys`）。
[[nodiscard]] constexpr bool opened_from_keys(LPARAM data) noexcept
{
    return data == -1;
}
} // namespace nenenib::ui::win32
