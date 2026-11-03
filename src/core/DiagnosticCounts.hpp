#pragma once

#include "Diagnostic.hpp"
#include "DiagnosticsCount.hpp"

#include <cstddef>
#include <span>

namespace nenenib::core
{
// ステータスバーの右端の件数（ADR 0161 の決定 4）。数えるのは error と warning だけで、
// information と hint は数えない。completeness は文書の診断を上限で打ち切ったか（件数に `+`）。
// 3 つは互いに独立して妥当なので公開 aggregate。比較は非メンバー（CPP-003）。
struct DiagnosticCounts
{
    std::size_t errors;
    std::size_t warnings;
    DiagnosticsCount completeness;
};

[[nodiscard]] constexpr bool operator==(const DiagnosticCounts &left,
                                        const DiagnosticCounts &right) noexcept
{
    return left.errors == right.errors && left.warnings == right.warnings &&
           left.completeness == right.completeness;
}

// 文書の診断の列を数える唯一の 1 本（ADR 0161 の決定 4 の追補）。frame を作る controller と
// ステータスバーの hit test の配置が同じ値を引く。
[[nodiscard]] DiagnosticCounts diagnostic_counts(std::span<const Diagnostic> diagnostics,
                                                 DiagnosticsCount completeness) noexcept;

// 件数の項目を出すか（error か warning が 1 件以上）。出さないときは項目の幅も取らない。
[[nodiscard]] constexpr bool counted(const DiagnosticCounts &counts) noexcept
{
    return counts.errors + counts.warnings > 0;
}
} // namespace nenenib::core
