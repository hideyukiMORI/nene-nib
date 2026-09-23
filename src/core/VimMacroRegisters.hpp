#pragma once

#include "VimKey.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::core
{
// マクロのレジスタの本数（a〜z・ADR 0046 の決定 1）。
inline constexpr std::size_t vim_macro_register_count = 26;

// マクロのレジスタ a〜z（ADR 0046 の決定 1）。録った鍵の列を名前ごとに 1 本ずつ持つ。
// 本文を持つ無名レジスタ（VimRegister）とは別の表で、`"ap` との統合は後続の ADR が決める。
struct VimMacroRegisters
{
    std::array<std::vector<VimKey>, vim_macro_register_count> keys;
};

// レジスタの名前 → 表の位置。小文字と大文字（追記）は同じ位置で、ほかの鍵は位置を持たない。
[[nodiscard]] constexpr std::optional<std::size_t> vim_macro_index(char32_t name) noexcept
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
