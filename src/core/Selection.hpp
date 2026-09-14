#pragma once

#include "Offset.hpp"
#include "OffsetRange.hpp"

namespace nenenib::core
{
// 選択は anchor と caret の 2 つの位置（ADR 0009 の決定 4）。anchor == caret は選択なし。
// どちらの位置も単独で妥当なので公開 aggregate（ADR 0007）。
struct Selection
{
    Offset anchor;
    Offset caret;
};

[[nodiscard]] constexpr bool operator==(const Selection &left, const Selection &right) noexcept
{
    return left.anchor == right.anchor && left.caret == right.caret;
}

[[nodiscard]] constexpr OffsetRange selection_range(const Selection &selection) noexcept
{
    if (selection.caret < selection.anchor)
    {
        return OffsetRange{selection.caret, selection.anchor};
    }
    return OffsetRange{selection.anchor, selection.caret};
}

[[nodiscard]] constexpr bool has_selection(const Selection &selection) noexcept
{
    return !(selection.anchor == selection.caret);
}

[[nodiscard]] constexpr Selection collapsed_at(Offset caret) noexcept
{
    return Selection{caret, caret};
}
} // namespace nenenib::core
