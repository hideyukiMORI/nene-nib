#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::core
{
// 論理行と、その行内の画面行（0始まり）。本文のOffsetとは別の位置である。
struct VisualPosition
{
    LineNumber line;
    std::size_t row;
};

[[nodiscard]] constexpr bool operator==(VisualPosition left, VisualPosition right) noexcept
{
    return left.line == right.line && left.row == right.row;
}

[[nodiscard]] constexpr bool operator<(VisualPosition left, VisualPosition right) noexcept
{
    return left.line < right.line || (left.line == right.line && left.row < right.row);
}
} // namespace nenenib::core
