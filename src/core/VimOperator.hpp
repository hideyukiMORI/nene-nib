#pragma once

#include <cstdint>

namespace nenenib::core
{
// 保留中のオペレータの閉じた一覧（ADR 0012 の決定 1 / ADR 0015 の決定 2）。範囲の規則は
// 3 つで同じ 1 本で、違うのは「消すか・消して INSERT へ入るか・本文を変えないか」だけ。
// > < と gu gU は次の縦切り。
enum class VimOperator : std::uint8_t
{
    remove,
    change,
    yank
};
} // namespace nenenib::core
