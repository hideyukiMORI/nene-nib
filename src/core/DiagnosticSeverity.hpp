#pragma once

#include <cstdint>

namespace nenenib::core
{
// 言語サーバーの診断の重さ（LSP 3.17 の DiagnosticSeverity の 1〜4・ADR 0161 の決定 2）。
// 並びは重い順で、行の写しで区間が重なったときは重い方だけを残す（決定 4）。
enum class DiagnosticSeverity : std::uint8_t
{
    error,
    warning,
    information,
    hint
};

// left が right より重いか（error > warning > information > hint）。
[[nodiscard]] constexpr bool is_heavier(DiagnosticSeverity left, DiagnosticSeverity right) noexcept
{
    return static_cast<std::uint8_t>(left) < static_cast<std::uint8_t>(right);
}
} // namespace nenenib::core
