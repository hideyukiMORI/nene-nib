#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// Ctrl+P の面の行の欄の文字組みが layout に焼く違い（#900）。同じ文字列・書式・寸法でも
// 焼き方が違えば別の資源にする。plain は名前と鍵、trimmed は場所（文字単位の省略記号）、
// trailing は右端の補足（右寄せ）。
enum class PaletteTextRole : std::uint8_t
{
    plain,
    trimmed,
    trailing,
};
} // namespace nenenib::ui::win32
