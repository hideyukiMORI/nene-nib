#pragma once

#include <cstdint>
#include <string>

namespace nenenib::core
{
// 答えの `error`（JSON-RPC 2.0・ADR 0160 の決定 5）。`data` は読まない。
struct LspError
{
    std::int64_t code;
    std::string message;
};
} // namespace nenenib::core
