#pragma once

#include "VimReplayKind.hpp"

#include <cstdint>

namespace nenenib::application
{
// 再生の列の 1 鍵の入れ子の深さと種類（ADR 0046 の決定 4・ADR 0176 の追補 33・#1312）。macro の
// 鍵は流す時点の `map` の表で引き、repeat（`.`）の鍵は引かない。深さは上限 100 の次まで
// しか数えないので 32 bit で足り、列の要素は今までの深さ（std::size_t）と同じ大きさのまま。
struct ReplayLevel
{
    std::uint32_t depth;
    core::VimReplayKind kind;
};
} // namespace nenenib::application
