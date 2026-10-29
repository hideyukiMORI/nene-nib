#pragma once

#include <cstdint>

namespace nenenib::core
{
// `map` の Ex の要求を設定へ写せない理由（ADR 0176 の決定 6・追補 15）。どれも 1 行の知らせで、
// 設定は変えない。no_such_mapping は `:unmap` 系の左辺がどの行にも無いとき（Vim の E31）。
// 値が増えたら知らせの switch が落ちる（CPP-002）。
enum class ExMapFailure : std::uint8_t
{
    unknown_key,
    empty_keys,
    long_lhs,
    bad_leader,
    unwritable,
    no_such_mapping
};
} // namespace nenenib::core
