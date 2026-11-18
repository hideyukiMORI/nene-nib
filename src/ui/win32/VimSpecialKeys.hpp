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

// 表に無い仮想キーは空（呼ぶ側は何も送らない）。Shift+Delete も空: Vim の `<S-Del>` は `<Del>`
// と別の鍵で、通常モードの Shift+Delete（切り取り）とも紛れるので、鍵を増やさない（#1325 の
// レビュー L2）。
[[nodiscard]] constexpr std::optional<core::VimSpecialKey> vim_special_for(WPARAM key,
                                                                           bool shifted) noexcept
{
    if (shifted && key == VK_DELETE)
    {
        return std::nullopt;
    }
    for (const KeyVimSpecial entry : vim_specials)
    {
        if (entry.key == key)
        {
            return entry.special;
        }
    }
    return std::nullopt;
}

// Vim の INSERT の補完の鍵（#1845・ADR 0169 の決定 3）。`Ctrl+N` は Vim の INSERT で常に engine の
// `Ctrl-n`（補完を始める・次の候補）。`Ctrl+P` `Ctrl+E` `Ctrl+Y` は補完の面が開いている間だけ engine
// の鍵で、閉じていれば空を返して今までの道へ流す（`Ctrl+P` はファイルの一覧・`Ctrl+E` `Ctrl+Y` は
// 通常モードの表）。control は Ctrl だけを押していること（Shift か Alt を組んだ鍵は写さない）、
// inserting は Vim の INSERT で本文に焦点があること、face_open は補完の面が開いていること。
[[nodiscard]] constexpr std::optional<core::VimSpecialKey>
vim_completion_key(WPARAM key, bool control, bool inserting, bool face_open) noexcept
{
    if (!control || !inserting)
    {
        return std::nullopt;
    }
    if (key == 'N')
    {
        return core::VimSpecialKey::control_n;
    }
    if (!face_open)
    {
        return std::nullopt;
    }
    constexpr std::array<KeyVimSpecial, 3> open_keys{{{'P', core::VimSpecialKey::control_p},
                                                     {'E', core::VimSpecialKey::control_e},
                                                     {'Y', core::VimSpecialKey::control_y}}};
    for (const KeyVimSpecial entry : open_keys)
    {
        if (entry.key == key)
        {
            return entry.special;
        }
    }
    return std::nullopt;
}
} // namespace nenenib::ui::win32
