#pragma once

#include "VimRegister.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 名前つきレジスタの本数（a〜z・ADR 0048 の決定 1）。
inline constexpr std::size_t vim_register_count = 26;

// 名前つきレジスタ a〜z（ADR 0048 の決定 1）。本文と種類を名前ごとに 1 本ずつ持つ表で、
// マクロもここに本文として置く（録画の鍵列は止めるときに本文へ写す・決定 6）。空のレジスタは
// VimRegister{"", uninitialized}。無名レジスタは VimState.unnamed_register が別に持つ。
struct VimNamedRegisters
{
    std::array<VimRegister, vim_register_count> registers;
};

// レジスタの名前 → 表の位置。小文字と大文字（追記）は同じ位置で、ほかの鍵は位置を持たない。
[[nodiscard]] constexpr std::optional<std::size_t> vim_register_index(char32_t name) noexcept
{
    if (name >= U'a' && name <= U'z')
    {
        return static_cast<std::size_t>(name - U'a');
    }
    if (name >= U'A' && name <= U'Z')
    {
        return static_cast<std::size_t>(name - U'A');
    }
    return std::nullopt;
}
} // namespace nenenib::core
