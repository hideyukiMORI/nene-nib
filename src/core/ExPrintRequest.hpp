#pragma once

#include <cstdint>

namespace nenenib::core
{
// `:ha[rdcopy]`（ADR 0177 の決定 1）。アクティブな文書の印刷の要求で、controller は F1 の一覧の
// 「印刷」と同じ意図 PrintDocument の 1 本へ渡す。引数（`> file.ps` など）・`!`・範囲の形は
// unsupported_argument（決定 9 の後続）。値が増えたら写し先の switch
// がコンパイルで落ちる（CPP-002）。
enum class ExPrintRequest : std::uint8_t
{
    active_document
};
} // namespace nenenib::core
