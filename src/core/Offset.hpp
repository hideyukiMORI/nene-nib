#pragma once

#include <cstddef>

namespace nenenib::core
{
// 本文の中のバイト位置（0 始まり・末尾の直後まで取り得る）。単位を型で持つ（CPP-001 / ADR 0009）。
// 値は単独で妥当なので公開 aggregate。比較は非メンバーで書く（CPP-003・Issue #3 の実測）。
struct Offset
{
    std::size_t value;
};

[[nodiscard]] constexpr bool operator==(Offset left, Offset right) noexcept
{
    return left.value == right.value;
}

[[nodiscard]] constexpr bool operator<(Offset left, Offset right) noexcept
{
    return left.value < right.value;
}
} // namespace nenenib::core
