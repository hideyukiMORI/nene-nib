#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// 状態集合の道（VimPatternEvaluation）の 1 文字あたりの費用（#1584・ADR 0184 の決定 1）。当たらない
// 本文（`a` だけの 1 行 16,000 字と、2,000 行 × 40 字）に、`*` を k 個含むパターンを当てる。
// lead は `z` の後ろに `a*` × k（先頭の原子が当たらないので生きた状態は 1 つ・review V-1570 の
// `/z` ＋ `a*` × k の形）、tail は `a*` × k の後ろに `c`（k 個の状態が全部生きる）、quantity は
// 量を開いた `a\{,1000}c`（#1588 の F3）。
enum class PatternCostWorkload : std::uint8_t
{
    lead_16_line,
    lead_64_line,
    lead_256_line,
    lead_1024_line,
    tail_16_line,
    tail_64_line,
    tail_256_line,
    tail_1024_line,
    quantity_line,
    lead_16_lines,
    lead_64_lines,
    lead_256_lines,
    lead_1024_lines,
    tail_16_lines,
    tail_64_lines,
    tail_256_lines,
    tail_1024_lines,
    quantity_lines
};
} // namespace nenenib::tests::performance
