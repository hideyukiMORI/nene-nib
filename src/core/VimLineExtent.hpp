#pragma once

#include <cstddef>

namespace nenenib::core
{
// 行単位 VISUAL で行った変更の「範囲の大きさ」（ADR 0033 の決定 5）。行数だけで、桁は持たない
// （Vim も行単位の再生では同じ行数だけを使う・Issue #91 で実測）。
struct VimLineExtent
{
    std::size_t lines;
};

[[nodiscard]] constexpr bool operator==(VimLineExtent left, VimLineExtent right) noexcept
{
    return left.lines == right.lines;
}
} // namespace nenenib::core
