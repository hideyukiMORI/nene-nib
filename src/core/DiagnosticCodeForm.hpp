#pragma once

#include <cstdint>

namespace nenenib::core
{
// 診断の `code` の JSON の型（#2005・ADR 0194 の決定 8）。LSP 3.17 の `integer | string` で、
// クイックフィックスを頼むときに `context.diagnostics` へ同じ型で戻す（ts は数の code で直し方を
// 照らす）。none は欄が無い・読めない（Diagnostic.code が空）。
enum class DiagnosticCodeForm : std::uint8_t
{
    none,
    number,
    text
};
} // namespace nenenib::core
