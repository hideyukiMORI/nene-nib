#pragma once

#include "Offset.hpp"

namespace nenenib::core
{
// バイト位置の半開区間 [begin, end)。選択と削除の範囲を 1 つの値で運ぶ（CPP-006）。
struct OffsetRange
{
    Offset begin;
    Offset end;
};

[[nodiscard]] constexpr bool operator==(const OffsetRange &left, const OffsetRange &right) noexcept
{
    return left.begin == right.begin && left.end == right.end;
}

[[nodiscard]] constexpr bool is_empty(const OffsetRange &range) noexcept
{
    return range.begin == range.end;
}
} // namespace nenenib::core
