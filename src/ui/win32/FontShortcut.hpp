#pragma once

#include "FontSizeAdjustment.hpp"

#include <windows.h>

#include <optional>

namespace nenenib::ui::win32
{
[[nodiscard]] inline std::optional<core::FontSizeAdjustment> font_shortcut(WPARAM key)
{
    // OS の仮想キーは開いた集合（CPP-017）。Ctrl は呼び出し側で確認済み。
    switch (key)
    {
    case VK_OEM_PLUS:
    case VK_ADD:
        return core::FontSizeAdjustment::increase;
    case VK_OEM_MINUS:
    case VK_SUBTRACT:
        return core::FontSizeAdjustment::decrease;
    case '0':
    case VK_NUMPAD0:
        return core::FontSizeAdjustment::reset;
    default:
        return std::nullopt;
    }
}
} // namespace nenenib::ui::win32
