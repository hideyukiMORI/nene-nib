#pragma once

#include "Column.hpp"
#include "DiagnosticSeverity.hpp"

namespace nenenib::core
{
// 1 行の中の診断の下線の区間（ADR 0161 の決定 4）。桁は ColorSpan と同じ code point の序数
// （行頭が Column{1}）の半開区間 [begin, end)。行の中では昇順で重ならない。
struct DiagnosticSpan
{
    Column begin;
    Column end;
    DiagnosticSeverity severity;
};

[[nodiscard]] constexpr bool operator==(const DiagnosticSpan &left,
                                        const DiagnosticSpan &right) noexcept
{
    return left.begin == right.begin && left.end == right.end && left.severity == right.severity;
}
} // namespace nenenib::core
