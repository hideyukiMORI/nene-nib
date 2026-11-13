#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// 命令の列の道（ADR 0184 の決定 3・#1603）の費用。`a` だけの 4,096 字の 1 行に、当たらない
// `\(a\|b\)*c`（約 10 命令・輪の中の選択）と `\(ab\|a\)\{,50}c`（約 400 命令・群の写し 50 個）を
// 当てる。量の無いパターンの道の費用は pattern- の 7 本が見る（決定 1）。
enum class PatternProgramWorkload : std::uint8_t
{
    branch_star_miss_4096,
    group_bounded_miss_4096
};
} // namespace nenenib::tests::performance
