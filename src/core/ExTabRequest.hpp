#pragma once

#include "ExTabVerb.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// Ex のタブの命令と引数の数（ADR 0057 の決定 4）。number は `:tabnext N` `:tabprevious N` の N で、
// ほかの命令では空。範囲の外かどうかは本数を知っている controller が tab_destination で決める。
struct ExTabRequest
{
    ExTabVerb verb;
    std::optional<std::size_t> number;
};
} // namespace nenenib::core
