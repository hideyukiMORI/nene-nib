#pragma once

#include "KeyVimSpecial.hpp"
#include "VimSpecialKey.hpp"

#include <windows.h>

#include <array>
#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
// Vim モードの仮想キー → 特別な鍵。通常モードの表と入れ替えて引く（ADR 0012 の決定 4）。
// Delete は engine の `<Del>`（#1325）。通常モードの DeleteText（ADR 0065 の決定 6）は送らない。
// 窓を作らずに試せるよう、表と引く関数をここに置く（nib_window_tests の `--vim-keys`）。
inline constexpr std::array<KeyVimSpecial, 12> vim_specials{
    {{VK_ESCAPE, core::VimSpecialKey::escape},
     {VK_RETURN, core::VimSpecialKey::enter},
     {VK_BACK, core::VimSpecialKey::backspace},
     {VK_LEFT, core::VimSpecialKey::arrow_left},
     {VK_RIGHT, core::VimSpecialKey::arrow_right},
     {VK_UP, core::VimSpecialKey::arrow_up},
     {VK_DOWN, core::VimSpecialKey::arrow_down},
     {VK_HOME, core::VimSpecialKey::home},
     {VK_END, core::VimSpecialKey::end},
     {VK_PRIOR, core::VimSpecialKey::page_up},
     {VK_NEXT, core::VimSpecialKey::page_down},
     {VK_DELETE, core::VimSpecialKey::delete_key}}};

// 表に無い仮想キーは空（呼ぶ側は何も送らない）。
[[nodiscard]] constexpr std::optional<core::VimSpecialKey> vim_special_for(WPARAM key) noexcept
{
    for (const KeyVimSpecial entry : vim_specials)
    {
        if (entry.key == key)
        {
            return entry.special;
        }
    }
    return std::nullopt;
}
} // namespace nenenib::ui::win32
