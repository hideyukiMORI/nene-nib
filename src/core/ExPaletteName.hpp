#pragma once

#include "PaletteScope.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
// Ex のファイル命令の表の 1 行。省略の最短長は Vim 9.1 の probe に基づく（ADR 0064）。
struct ExPaletteName
{
    std::string_view name;
    std::size_t shortest;
    PaletteScope scope;
};
} // namespace nenenib::core
