#pragma once

#include "DiagnosticSeverity.hpp"
#include "OffsetRange.hpp"

#include <string>

namespace nenenib::core
{
// 文書の値としての診断 1 件（ADR 0161 の決定 2）。range は本文のバイト位置の半開区間、message は
// 改行の前までの 1 行。3 つは互いに独立して妥当なので公開 aggregate。比較は非メンバー（CPP-003）。
struct Diagnostic
{
    OffsetRange range;
    DiagnosticSeverity severity;
    std::string message;
};

[[nodiscard]] inline bool operator==(const Diagnostic &left, const Diagnostic &right)
{
    return left.range == right.range && left.severity == right.severity &&
           left.message == right.message;
}
} // namespace nenenib::core
