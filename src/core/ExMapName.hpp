#pragma once

#include "ExMapVerb.hpp"
#include "VimModeSet.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
// `map` の Ex の命令の名前の表の 1 行（ADR 0176 の決定 6・CPP-012）。name は完全な形、shortest は
// Vim 9.1 の最短の省略の長さで、modes はその命令が効くモード、remap が偽なら noremap 系。
struct ExMapName
{
    std::string_view name;
    std::size_t shortest;
    ExMapVerb verb;
    VimModeSet modes;
    bool remap;
};
} // namespace nenenib::core
