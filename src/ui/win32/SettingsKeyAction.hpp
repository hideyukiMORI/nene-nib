#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// 設定の面が開いている間の鍵の行き先（施主決定 D102・#1836）。swallow は何もしない（本文へも
// 通さない）。open_choice は焦点の行の選ぶ欄の一覧を開く（F4・#1926）。
enum class SettingsKeyAction : std::uint8_t
{
    swallow,
    close,
    revert,
    step_up,
    step_down,
    page_up,
    page_down,
    first,
    last,
    change,
    previous_value,
    next_value,
    erase,
    open_choice
};
} // namespace nenenib::ui::win32
