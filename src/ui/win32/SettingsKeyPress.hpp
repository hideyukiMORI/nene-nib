#pragma once

#include <windows.h>

namespace nenenib::ui::win32
{
// 設定の面が開いている間の 1 つの WM_KEYDOWN（#1836）。control は Ctrl（Alt と一緒の AltGr は
// 偽）、repeated は長押しの反復（lParam の bit 30）。公開 aggregate（CPP-003）。
struct SettingsKeyPress
{
    WPARAM key;
    bool control;
    bool shift;
    bool repeated;
};
} // namespace nenenib::ui::win32
