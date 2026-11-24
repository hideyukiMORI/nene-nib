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

// Vim の INSERT の補完の鍵（#1845・ADR 0169 の決定 3）。`Ctrl+N` は Vim の INSERT で常に
// engine の `Ctrl-n`（補完を始める・次の候補）。`Ctrl+P` `Ctrl+E` `Ctrl+Y` は補完の面が
// 見えている間だけ engine の鍵で、見えていなければ空を返して今までの道へ流す（`Ctrl+P` は
// ファイルの一覧・`Ctrl+E` `Ctrl+Y` は通常モードの表）。言語サーバーの答えを待っているだけで
// 面がまだ見えていない間も、見えていない側（#1850 の S2）。
// control は Ctrl だけを押していること（Shift か Alt を組んだ鍵は写さない）。
// inserting は Vim の INSERT で本文に焦点があること。face_shown は補完の面が見えていること。
[[nodiscard]] constexpr std::optional<core::VimSpecialKey>
vim_completion_key(WPARAM key, bool control, bool inserting, bool face_shown) noexcept
{
    if (!control || !inserting)
    {
        return std::nullopt;
    }
    if (key == 'N')
    {
        return core::VimSpecialKey::control_n;
    }
    if (!face_shown)
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

// Vim のモードの Tab の前に補完の面を閉じるか（#1971）。打鍵で自動に開いた面の Tab は確定の鍵だが、
// 確定するのは修飾の無い Tab だけで、Shift+Tab は前のまま（面を閉じて Tab の文字を入れる）。engine
// へは同じ Tab の文字の鍵で渡るので、窓が先に面を閉じる。
[[nodiscard]] constexpr bool tab_closes_completion(bool shifted, bool face_open) noexcept
{
    return shifted && face_open;
}
} // namespace nenenib::ui::win32
