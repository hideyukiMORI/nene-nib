#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// 選ぶ欄の一覧が開いている間の鍵の行き先（#1926・採用案 2026-11-24）。swallow は何もしない
// （面へも本文へも通さない）。previous / next は 1 つ（端で止まる）、first / last は端、
// page_up / page_down は 1 ページ、choose は決めて閉じる（Enter・Tab / Shift+Tab）、
// cancel は決めずに閉じる（Esc・F4）、close_panel は面ごと閉じる（Ctrl+,・一覧は決めずに
// 閉じる）、erase は打った字の 1 つ（Backspace）。
enum class SettingsChoiceKeyAction : std::uint8_t
{
    swallow,
    previous,
    next,
    first,
    last,
    page_up,
    page_down,
    choose,
    cancel,
    close_panel,
    erase
};
} // namespace nenenib::ui::win32
