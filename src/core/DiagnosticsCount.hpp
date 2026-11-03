#pragma once

#include <cstdint>

namespace nenenib::core
{
// 届いた診断を全部持ったか、上限（lsp_diagnostics_limit）で打ち切ったか（ADR 0161 の決定 2・
// 打ち切りは件数に `+` を付ける）。
enum class DiagnosticsCount : std::uint8_t
{
    complete,
    truncated
};
} // namespace nenenib::core
