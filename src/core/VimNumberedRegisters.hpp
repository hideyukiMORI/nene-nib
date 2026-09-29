#pragma once

#include "VimRegister.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 数字レジスタの本数（`"0`〜`"9`・ADR 0050 の決定 1）。
inline constexpr std::size_t vim_numbered_count = 10;

// 数字レジスタ `"0`〜`"9`（ADR 0050 の決定 1）。位置 0 が `"0`（yank）、1〜9 が削除の繰り下がり。
// 空のレジスタは VimRegister{"", uninitialized}。書くのは registers_written の 1 本だけ。
struct VimNumberedRegisters
{
    std::array<VimRegister, vim_numbered_count> registers;
};

// 数字の名前 → 表の位置。`0`〜`9` だけが位置を持つ。
[[nodiscard]] constexpr std::optional<std::size_t> vim_numbered_index(char32_t name) noexcept
{
    if (name >= U'0' && name <= U'9')
    {
        return static_cast<std::size_t>(name - U'0');
    }
    return std::nullopt;
}
} // namespace nenenib::core
