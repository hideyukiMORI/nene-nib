#pragma once

#include "KeyChord.hpp"
#include "OperationKey.hpp"

#include <windows.h>

#include <optional>

namespace nenenib::ui::win32
{
// 仮想キー → 操作の鍵の写し（ADR 0078 の決定 2・13）。何が起きるかは core::operation_for が決める。
// 写すのは Ctrl つきの O S P Z Y + - 0 Space \、Ctrl+Shift+F4、修飾なしの F1、Ctrl なしの F6 だけ。
// 主キーとテンキーの + - 0 は同じ値。`\` は JIS 配列の 2 つの物理キー（VK_OEM_5 の ¥ と
// VK_OEM_102 の ろ）を同じ値にする（ADR 0148 の決定 6）。Shift は押されているままを入れる（表に
// Shift つきの行がある）。Alt のときは呼ばない。T W D Tab と Shift なしの F4 は写さない（タブと
// ブックマークの鍵は TabShortcut と BookmarkKey の経路のまま）。
// F1 は修飾なしだけ、F6 は Ctrl なし（Shift はそのまま入れる）。
[[nodiscard]] inline std::optional<core::KeyChord> function_chord(WPARAM key, bool control,
                                                                  bool shift)
{
    if (control)
    {
        return std::nullopt;
    }
    if (key == VK_F6)
    {
        return core::KeyChord{false, shift, core::OperationKey::f6};
    }
    if (key == VK_F1 && !shift)
    {
        return core::KeyChord{false, false, core::OperationKey::f1};
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<core::KeyChord> operation_chord(WPARAM key, bool control,
                                                                   bool shift)
{
    if (key == VK_F1 || key == VK_F6)
    {
        return function_chord(key, control, shift);
    }
    if (!control)
    {
        return std::nullopt;
    }
    const auto chord = [shift](core::OperationKey operation_key)
    { return std::optional{core::KeyChord{true, shift, operation_key}}; };
    // OS の仮想キーは開いた集合（CPP-017）。
    switch (key)
    {
    case 'O':
        return chord(core::OperationKey::o);
    case 'S':
        return chord(core::OperationKey::s);
    case 'P':
        return chord(core::OperationKey::p);
    case 'Z':
        return chord(core::OperationKey::z);
    case 'Y':
        return chord(core::OperationKey::y);
    case VK_OEM_PLUS:
    case VK_ADD:
        return chord(core::OperationKey::plus);
    case VK_OEM_MINUS:
    case VK_SUBTRACT:
        return chord(core::OperationKey::minus);
    case '0':
    case VK_NUMPAD0:
        return chord(core::OperationKey::zero);
    case VK_SPACE:
        return chord(core::OperationKey::space);
    case VK_OEM_5:
    case VK_OEM_102:
        return chord(core::OperationKey::backslash);
    case VK_F4:
        return shift ? chord(core::OperationKey::f4) : std::nullopt;
    default:
        return std::nullopt;
    }
}
} // namespace nenenib::ui::win32
