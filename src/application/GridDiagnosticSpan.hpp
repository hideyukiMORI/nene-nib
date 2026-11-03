#pragma once

#include "DiagnosticSeverity.hpp"
#include "GridCellSpan.hpp"

namespace nenenib::application
{
// 格子の 1 画面行の診断の下線の升の範囲（ADR 0161 の決定 4）。画面行の中では昇順で重ならず、
// 変換中の文字列を差し込んだ升には掛からない（構文の色 GridColorSpan と同じ写し）。
struct GridDiagnosticSpan
{
    GridCellSpan cells;
    core::DiagnosticSeverity severity;
};

[[nodiscard]] constexpr bool operator==(GridDiagnosticSpan left, GridDiagnosticSpan right) noexcept
{
    return left.cells == right.cells && left.severity == right.severity;
}
} // namespace nenenib::application
