#pragma once

#include <cstddef>

namespace nenenib::core
{
// 鍵の前に積まれた回数（3j の 3）。0 は回数にならない（ADR 0012 の決定 1）。
struct VimCount
{
    std::size_t value;
};

[[nodiscard]] constexpr bool operator==(VimCount left, VimCount right) noexcept
{
    return left.value == right.value;
}
} // namespace nenenib::core
