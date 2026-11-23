#pragma once

#include <cstdint>

namespace nenenib::core
{
// インデントガイドの段の幅（ADR 0193 の決定 7）。開いたときに本文の先頭から推し当てる値で、
// 文書ごとに持つ（設定の indent_guide_width は推し当てが外れたときの値）。tab の段の幅は
// vim_tab_stop。表示だけの値で、`>>` `<<` の vim_shift_width とは別（決定 8）。
enum class IndentUnit : std::uint8_t
{
    two,
    four,
    eight,
    tab
};
} // namespace nenenib::core
