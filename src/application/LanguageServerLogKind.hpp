#pragma once

#include <cstdint>

namespace nenenib::application
{
// 記録の 1 行の出どころ（ADR 0160 の 2026-11-22 の追補・LSP 段 S8）。output は子の標準エラー、
// ほかは `window/showMessage` と `window/logMessage` の type。行の頭の印（`[stderr]` `[log]`
// など）は LanguageServerLog の 1 か所が書く。
enum class LanguageServerLogKind : std::uint8_t
{
    output,
    error,
    warning,
    info,
    log
};
} // namespace nenenib::application
