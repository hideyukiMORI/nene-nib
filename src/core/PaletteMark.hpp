#pragma once

#include "PaletteScope.hpp"

#include <string_view>

namespace nenenib::core
{
// 出どころの記号の表の 1 行（ADR 0060 の決定 2・CPP-012）。mark は入力の先頭の 1 文字、name は
// 検索欄の案内に出す名前。
struct PaletteMark
{
    char mark;
    PaletteScope scope;
    std::string_view name;
};
} // namespace nenenib::core
