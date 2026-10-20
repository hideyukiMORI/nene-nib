#pragma once

#include <cstdint>

namespace nenenib::core
{
// 保留中のオペレータの閉じた一覧（ADR 0012 の決定 1 / ADR 0015 の決定 2）。範囲の規則は
// 3 つで同じ 1 本で、違うのは「消すか・消して INSERT へ入るか・本文を変えないか」だけ。
// g~ gu gU は同じ範囲の ASCII の英字の大小を変え、> < は範囲が覆う行の字下げを変える
// （ADR 0015 の決定 10・11）。どれもレジスタを書かない。
enum class VimOperator : std::uint8_t
{
    remove,
    change,
    yank,
    toggle_case,
    lower_case,
    upper_case,
    shift_right,
    shift_left
};
} // namespace nenenib::core
