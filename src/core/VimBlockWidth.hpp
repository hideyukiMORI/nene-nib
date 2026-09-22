#pragma once

#include <cstddef>

namespace nenenib::core
{
// 矩形が覆う桁の数（ADR 0035 の決定 5）。桁そのものではないので `VirtualColumn` とは別の型に
// して混ぜられなくする（CPP-001）。1 以上で、`$` で取った矩形でも固定 Vim は数を残す
// （`getregtype()` の `^V{幅}` が常に有限・Issue #112 で実測）。
struct VimBlockWidth
{
    std::size_t columns;
};

[[nodiscard]] constexpr bool operator==(VimBlockWidth left, VimBlockWidth right) noexcept
{
    return left.columns == right.columns;
}
} // namespace nenenib::core
