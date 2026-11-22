#pragma once

#include <cstdint>

namespace nenenib::core
{
// `$/progress` の WorkDoneProgress の kind（LSP 3.17・ADR 0160 の 2026-11-22 の追補）。
enum class LspProgressKind : std::uint8_t
{
    begin,
    report,
    end
};
} // namespace nenenib::core
