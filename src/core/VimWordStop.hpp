#pragma once

#include <cstdint>

namespace nenenib::core
{
// w がどこで止まるか。オペレータの後ろの w は行末で止まり、次の行へは渡らない
// （Vim の word-motions の特例。Issue #22 で実測）。素の w は行をまたぐ。
enum class VimWordStop : std::uint8_t
{
    across_lines,
    at_line_end
};
} // namespace nenenib::core
