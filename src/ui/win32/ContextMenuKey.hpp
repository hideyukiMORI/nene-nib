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

// WM_SYSCOMMAND を既定の処理へ渡さずに捨てるか（#1051・実機の V-1791 の後の切り分け）。Shift+F10 の
// F10 の離しは既定の処理で SC_KEYMENU（窓の「メニューのモード」）になり、開いた自前のメニューの
// 1 回目の Esc をそのモードが食べる。捨てるのは SC_KEYMENU
// のうち、自前のメニューが開いているときと、 Shift つきの鍵の分（lParam が 0・F10 の離し）だけ。F10
// 単独と Alt（Shift なし）と Alt+文字（lParam が文字）は今のまま既定の処理へ渡す。menu_open
// はメニューが開いているか、shifted は Shift の押し。
[[nodiscard]] constexpr bool swallows_key_menu(WPARAM word, LPARAM data, bool menu_open,
                                               bool shifted) noexcept
{
    constexpr WPARAM command_mask = 0xFFF0;
    return (word & command_mask) == SC_KEYMENU && (menu_open || (shifted && data == 0));
}
} // namespace nenenib::ui::win32
