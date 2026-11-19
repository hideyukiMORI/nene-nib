#pragma once

#include "LspRequestKind.hpp"
#include "PendingLspContext.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// 答えを待っている要求 1 つ（ADR 0160 の決定 5・6・ADR 0190 の決定 1）。id は controller の
// 通し番号、context は答えを照合する文脈（寿命の要求は文脈なし）、ticks は送ってから数えた
// LanguageServerTick の回数（時刻は読まない）。公開 aggregate（CPP-003）。
struct PendingLspRequest
{
    std::int64_t id;
    LspRequestKind kind;
    PendingLspContext context = {};
    std::size_t ticks = 0;
};
} // namespace nenenib::application
