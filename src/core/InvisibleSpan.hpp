#pragma once

#include "Column.hpp"
#include "InvisibleKind.hpp"

namespace nenenib::core
{
// 1 行の中の不可視文字の区間（ADR 0193 の決定 1・10）。桁は ColorSpan と同じ code point の序数
// （行頭が Column{1}）の半開区間 [begin, end)。同じ種類の続きは 1 つの区間にまとめ、行の中では
// 昇順で重ならない。renderer は区間の 1 字ごとに印を描く。公開 aggregate（CPP-003）。
struct InvisibleSpan
{
    Column begin;
    Column end;
    InvisibleKind kind;
};

[[nodiscard]] constexpr bool operator==(const InvisibleSpan &left,
                                        const InvisibleSpan &right) noexcept
{
    return left.begin == right.begin && left.end == right.end && left.kind == right.kind;
}
} // namespace nenenib::core
