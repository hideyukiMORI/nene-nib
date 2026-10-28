#pragma once

#include "LspRequestKind.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// 答えを待っている要求 1 つ（ADR 0160 の決定 5・6）。id は controller の通し番号、ticks は送って
// から数えた LanguageServerTick の回数（時刻は読まない）。公開 aggregate（CPP-003）。
struct PendingLspRequest
{
    std::int64_t id;
    LspRequestKind kind;
    std::size_t ticks = 0;
};
} // namespace nenenib::application
