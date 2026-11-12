#pragma once

#include "VimPatternMatch.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 群の数の上限（Vim の NSUBEXP - 1・`\1`〜`\9`）。
inline constexpr std::size_t vim_pattern_group_limit = 9;

// 群の範囲を引く道の探索の升の数の上限（ADR 0168 の追補 13）。升 1 つが 1 bit の印と、積みの段が
// 多くとも 1 つ（16 バイト）なので、上限の一致でも確保は約 0.5 MiB の印と 32 MiB の積み、仕事は升の
// 数に比例する（`\(.*\),\(.*\)` なら約 1 Mi 字の一致まで）。超える一致の置換は断る（Vim の E363 と
// 同じ扱い・群を黙って空にしない）。
inline constexpr std::size_t vim_capture_cell_limit = std::size_t{4} * 1024U * 1024U;

// 一致 1 つの群の範囲（ADR 0168 の決定 4）。0 は一致の全体（`&` `\0`）、1〜9 は `\(` の順。
// 値が無い群は通らなかったか、パターンに無い（置換の `\N` は何も書かない・Vim と同じ）。
using VimPatternCaptures = std::array<std::optional<VimPatternMatch>, vim_pattern_group_limit + 1>;
} // namespace nenenib::core
