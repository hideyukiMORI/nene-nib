#pragma once

#include "SettingsKeyAction.hpp"

#include <windows.h>

#include <cstddef>
#include <optional>

namespace nenenib::ui::win32
{
// 設定の面が開いている間の WM_KEYDOWN の行き先（施主決定 D102・#1836）。面の中は通常モードでも
// Vim のモードでも同じ鍵で、ここに無い鍵は捨てる（本文へ通さない）。窓を作らずに試せるようここに
// 置く（nib_window_tests の `--settings-panel`）。
// - ↑ ↓ と Tab / Shift+Tab は行の移動、PageUp / PageDown は 1 ページ、Home / End は最初と最後の行。
// - Enter は切り替え（選ぶ欄は次の値）。Space は文字の側（WM_CHAR）が決める（探す欄が空なら
//   切り替え・打ちかけなら空白）。← → は値を前後へ。
// - Esc と Ctrl+, は閉じる（保存）。Ctrl+Z は元に戻す。Backspace は探す欄の 1 文字。
// OS の仮想キーは開いた集合（CPP-017）。
[[nodiscard]] constexpr SettingsKeyAction settings_key_action(WPARAM key, bool control,
                                                              bool shift) noexcept
{
    if (control)
    {
        switch (key)
        {
        case VK_OEM_COMMA:
            return SettingsKeyAction::close;
        case 'Z':
            return SettingsKeyAction::revert;
        default:
            return SettingsKeyAction::swallow;
        }
    }
    switch (key)
    {
    case VK_ESCAPE:
        return SettingsKeyAction::close;
    case VK_UP:
        return SettingsKeyAction::step_up;
    case VK_DOWN:
        return SettingsKeyAction::step_down;
    case VK_TAB:
        return shift ? SettingsKeyAction::step_up : SettingsKeyAction::step_down;
    case VK_PRIOR:
        return SettingsKeyAction::page_up;
    case VK_NEXT:
        return SettingsKeyAction::page_down;
    case VK_HOME:
        return SettingsKeyAction::first;
    case VK_END:
        return SettingsKeyAction::last;
    case VK_RETURN:
        return SettingsKeyAction::change;
    case VK_LEFT:
        return SettingsKeyAction::previous_value;
    case VK_RIGHT:
        return SettingsKeyAction::next_value;
    case VK_BACK:
        return SettingsKeyAction::erase;
    default:
        return SettingsKeyAction::swallow;
    }
}

// Ctrl+1〜6 は節を送る（札の順・主キーの数字）。ほかは値なし。
[[nodiscard]] constexpr std::optional<std::size_t> settings_section_key(WPARAM key,
                                                                        bool control) noexcept
{
    if (!control || key < '1' || key > '6')
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(key - '1');
}
} // namespace nenenib::ui::win32
