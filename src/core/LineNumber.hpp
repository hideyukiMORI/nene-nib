#pragma once

#include <cstddef>

namespace nenenib::core
{
// 行番号（1 始まり）。バイト位置と混ぜないための専用型（CPP-001 / ADR 0009）。
struct LineNumber
{
    std::size_t value;
};

[[nodiscard]] constexpr bool operator==(LineNumber left, LineNumber right) noexcept
{
    return left.value == right.value;
}

[[nodiscard]] constexpr bool operator<(LineNumber left, LineNumber right) noexcept
{
    return left.value < right.value;
}
} // namespace nenenib::core
