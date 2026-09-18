#pragma once

#include "VimCount.hpp"
#include "VimOperator.hpp"

#include <optional>

namespace nenenib::core
{
// 保留中のオペレータ（ADR 0015 の決定 1）。d c y を押した時点の回数を自分で持つ。
// そのあとに積んだ回数（VimState::count）とは掛け算で、2d3w は 6 語・2d3d は 6 行になる。
// Vim の opcount と count がこの形で、回数を 1 つに積むと 2d3w が 23 語になってしまう。
struct VimPendingOperator
{
    VimOperator operation;
    std::optional<VimCount> count;
};
} // namespace nenenib::core
