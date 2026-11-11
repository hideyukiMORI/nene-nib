#pragma once

#include "DeleteDirection.hpp"
#include "EditMode.hpp"

#include <windows.h>

#include <optional>

namespace nenenib::ui::win32
{
// Ctrl を押した Backspace / Delete が送る語の削除の向き（ADR 0009 の追補 #1394）。通常モードの
// Ctrl+Backspace が後ろ向き、Ctrl+Delete が前向き。Vim のモードと、Shift も押したとき
// （Ctrl+Shift+Backspace / Ctrl+Shift+Delete・メモ帳の実測が無いので鍵を増やさない・#1410 の
// レビュー L4）は空で、呼ぶ側は何も送らない。窓を作らずに試せるよう、引く関数をここに置く
// （nib_window_tests の `--word-delete`）。
[[nodiscard]] constexpr std::optional<core::DeleteDirection>
word_deletion_for(WPARAM key, bool shifted, core::EditMode mode) noexcept
{
    switch (mode)
    {
    case core::EditMode::vim:
        return std::nullopt;
    case core::EditMode::ordinary:
        break;
    }
    if (shifted)
    {
        return std::nullopt;
    }
    if (key == VK_BACK)
    {
        return core::DeleteDirection::backward;
    }
    if (key == VK_DELETE)
    {
        return core::DeleteDirection::forward;
    }
    return std::nullopt;
}
} // namespace nenenib::ui::win32
