#pragma once

#include "Column.hpp"
#include "SyntaxRole.hpp"

namespace nenenib::core
{
// 1 行の中の色の区間（#815・ADR 0156 の決定 2）。桁は SelectionSpan と同じ code point
// の序数（行頭が Column{1}）の半開区間 [begin, end)。行の中では昇順で重ならず、区間の無い所は本文の
// text の色。
struct ColorSpan
{
    Column begin;
    Column end;
    SyntaxRole role;
};

[[nodiscard]] constexpr bool operator==(const ColorSpan &left, const ColorSpan &right) noexcept
{
    return left.begin == right.begin && left.end == right.end && left.role == right.role;
}
} // namespace nenenib::core
