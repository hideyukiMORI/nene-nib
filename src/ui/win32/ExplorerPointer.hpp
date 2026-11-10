#pragma once

#include "ExplorerPointing.hpp"

#include <windows.h>

#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
// 面の行の上の左ボタンの窓メッセージ → 送る PointExplorer の種類（ADR 0162 の決定 4 の改め・
// #1388）。OS は面の上の速い 2 回の押しを「押す → 離す → DBLCLK → 離す」と送る（窓の class の
// CS_DBLCLKS）。押しと DBLCLK は面の上で押した印 pressed を立て、離しは印が立っているときだけ
// released にする（面の外で押して面の上で離しても送らない）。2 打目の意味（読まない・開閉しない）は
// application の状態機械が決め、窓は写すだけ。ほかのメッセージは面の行の押しではない。OS の
// メッセージ番号は開いた集合なので既定分岐を置く（CPP-017）。
[[nodiscard]] constexpr std::optional<application::ExplorerPointing>
explorer_pointing_of(UINT message, bool pressed) noexcept
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
        return application::ExplorerPointing::pressed;
    case WM_LBUTTONDBLCLK:
        return application::ExplorerPointing::double_click;
    case WM_LBUTTONUP:
        if (pressed)
        {
            return application::ExplorerPointing::released;
        }
        return std::nullopt;
    default:
        return std::nullopt;
    }
}

// 送った後に押した印が立っているか。押しと DBLCLK は次の離しを待ち、離しは印を消す。
[[nodiscard]] constexpr bool explorer_pressed_after(application::ExplorerPointing pointing) noexcept
{
    switch (pointing)
    {
    case application::ExplorerPointing::pressed:
    case application::ExplorerPointing::double_click:
        return true;
    case application::ExplorerPointing::released:
        return false;
    }
    std::unreachable();
}
} // namespace nenenib::ui::win32
