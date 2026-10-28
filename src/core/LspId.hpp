#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace nenenib::core
{
// JSON-RPC の要求の id（ADR 0160 の決定 5）。LSP 3.17 では整数か文字列。Nib が送る要求は
// application の通し番号（整数）で、サーバーからの要求は文字列の id も来るので両方を持つ。
using LspId = std::variant<std::int64_t, std::string>;
} // namespace nenenib::core
