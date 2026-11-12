#pragma once

#include "VimPatternMatch.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 群の数の上限（Vim の NSUBEXP - 1・`\1`〜`\9`）。
inline constexpr std::size_t vim_pattern_group_limit = 9;

// 一致 1 つの群の範囲（ADR 0168 の決定 4）。0 は一致の全体（`&` `\0`）、1〜9 は `\(` の順。
// 値が無い群は通らなかったか、パターンに無い（置換の `\N` は何も書かない・Vim と同じ）。
using VimPatternCaptures = std::array<std::optional<VimPatternMatch>, vim_pattern_group_limit + 1>;
} // namespace nenenib::core
