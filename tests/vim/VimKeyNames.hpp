// clang-format off
// 生成物。手で編集しない。python eng/vim-oracle.py --key-names
#pragma once

#include "VimKeyName.hpp"

#include <array>

namespace nenenib::tests
{
inline constexpr std::array<VimKeyName, 19> vim_key_names{{
    {"<Esc>", nenenib::core::VimSpecialKey::escape},
    {"<CR>", nenenib::core::VimSpecialKey::enter},
    {"<BS>", nenenib::core::VimSpecialKey::backspace},
    {"<C-r>", nenenib::core::VimSpecialKey::control_r},
    {"<C-d>", nenenib::core::VimSpecialKey::control_d},
    {"<C-u>", nenenib::core::VimSpecialKey::control_u},
    {"<C-f>", nenenib::core::VimSpecialKey::control_f},
    {"<C-b>", nenenib::core::VimSpecialKey::control_b},
    {"<C-v>", nenenib::core::VimSpecialKey::control_v},
    {"<NL>", nenenib::core::VimCharacter{U'\x0A'}},
    {"<Home>", nenenib::core::VimSpecialKey::home},
    {"<End>", nenenib::core::VimSpecialKey::end},
    {"<PageUp>", nenenib::core::VimSpecialKey::page_up},
    {"<PageDown>", nenenib::core::VimSpecialKey::page_down},
    {"<Space>", nenenib::core::VimCharacter{U'\x20'}},
    {"<Left>", nenenib::core::VimSpecialKey::arrow_left},
    {"<Right>", nenenib::core::VimSpecialKey::arrow_right},
    {"<Up>", nenenib::core::VimSpecialKey::arrow_up},
    {"<Down>", nenenib::core::VimSpecialKey::arrow_down},
}};
} // namespace nenenib::tests
// clang-format on
