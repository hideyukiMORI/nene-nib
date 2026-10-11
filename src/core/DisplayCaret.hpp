#pragma once

#include "CaretAffinity.hpp"
#include "Column.hpp"

namespace nenenib::core
{
// 表示文字列のcode pointの桁。本文への逆変換はDisplayLineの対応表だけで行う。
struct DisplayCaret
{
    Column column;
    CaretAffinity affinity = CaretAffinity::leading;
};
} // namespace nenenib::core
