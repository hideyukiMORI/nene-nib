#pragma once

#include <cstddef>

namespace nenenib::core
{
// 桁（code point 単位・1 始まり）。バイト数ではないので専用型で運ぶ（CPP-001 / ADR 0009）。
struct Column
{
    std::size_t value;
};

[[nodiscard]] constexpr bool operator==(Column left, Column right) noexcept
{
    return left.value == right.value;
}

[[nodiscard]] constexpr bool operator<(Column left, Column right) noexcept
{
    return left.value < right.value;
}
} // namespace nenenib::core
