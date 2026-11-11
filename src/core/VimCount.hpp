#pragma once

#include <cstddef>

namespace nenenib::core
{
// 鍵の前に積まれた回数（3j の 3）。0 は回数にならない（ADR 0012 の決定 1）。
struct VimCount
{
    std::size_t value;
};

// 回数の上限（Vim 9.1 の normal.c は打った桁と opcount との積を 999999999 に貼り付ける）。
inline constexpr std::size_t vim_count_limit = 999'999'999;

[[nodiscard]] constexpr bool operator==(VimCount left, VimCount right) noexcept
{
    return left.value == right.value;
}
} // namespace nenenib::core
