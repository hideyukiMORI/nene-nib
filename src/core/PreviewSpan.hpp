#pragma once

#include "Offset.hpp"
#include "PreviewStyle.hpp"

namespace nenenib::core
{
// 要素の text の中の装飾の半開区間 [begin, end)（ADR 0151 の決定 7）。位置は本文でなく要素の
// text のバイト位置。入れ子は外側が先に並ぶ（開いた順）。比較は非メンバー（CPP-003）。
struct PreviewSpan
{
    Offset begin;
    Offset end;
    PreviewStyle style;
};

[[nodiscard]] constexpr bool operator==(const PreviewSpan &left, const PreviewSpan &right) noexcept
{
    return left.begin == right.begin && left.end == right.end && left.style == right.style;
}
} // namespace nenenib::core
