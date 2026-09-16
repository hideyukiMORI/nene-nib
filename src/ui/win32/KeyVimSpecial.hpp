#pragma once

#include "VimSpecialKey.hpp"

#include <cstdint>

namespace nenenib::ui::win32
{
// 仮想キー → Vim の特別な鍵の表の 1 行（CPP-012）。通常モードの表とは別の表で、
// モードで表を切り替える。真偽値で経路を分けない（ADR 0012 の決定 4）。
struct KeyVimSpecial
{
    std::uint32_t key;
    core::VimSpecialKey special;
};
} // namespace nenenib::ui::win32
