#pragma once

#include "DiagnosticSeverity.hpp"
#include "OffsetRange.hpp"

namespace nenenib::application
{
// 格子の 1 行の診断の下線の区間を本文の位置で持つ（ADR 0161 の決定 4）。controller が行の
// DiagnosticSpan（本文の桁）を本文の位置へ写した値で、GridLineRows が当たりと同じ写しで画面行の
// 升へ切る。行の中では昇順で重ならない。
struct GridDiagnosticRange
{
    core::OffsetRange range;
    core::DiagnosticSeverity severity;
};
} // namespace nenenib::application
