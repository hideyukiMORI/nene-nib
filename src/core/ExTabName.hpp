#pragma once

#include "ExTabVerb.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
// タブの命令の名前の表の 1 行（ADR 0057 の決定 4・CPP-012）。name は完全な形、shortest は受ける
// 最短の省略の長さで、入力は「最短の形から完全な形までの前方一致」のとき verb になる。
struct ExTabName
{
    std::string_view name;
    std::size_t shortest;
    ExTabVerb verb;
};
} // namespace nenenib::core
