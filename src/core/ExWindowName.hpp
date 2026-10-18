#pragma once

#include "ExWindowCommand.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
// 窓の命令の名前の表の 1 行（ADR 0148 の決定 5・CPP-012）。ExTabName と同じ形で、入力は
// 「最短の形から完全な形までの前方一致」のとき command になる。
struct ExWindowName
{
    std::string_view name;
    std::size_t shortest;
    ExWindowCommand command;
};
} // namespace nenenib::core
