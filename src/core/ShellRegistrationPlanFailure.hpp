#pragma once

#include <cstdint>

namespace nenenib::core
{
// 登録の計画を作れない理由（ADR 0178 の決定 2・ARC-010）。exe のパスは `"` で囲むので、パスの
// 中の `"` は引用を壊す。空のパスは指す exe が無い。
enum class ShellRegistrationPlanFailure : std::uint8_t
{
    empty_path,
    quote_in_path
};
} // namespace nenenib::core
