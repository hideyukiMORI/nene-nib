#pragma once

#include "VimReplayKind.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// 再生の列の 1 鍵の入れ子の深さと種類（ADR 0046 の決定 4・ADR 0176 の追補 33・#1312）。macro の
// 鍵は流す時点の `map` の表で引き、repeat（`.`）の鍵は引かない。深さは上限 100 の次まで
// しか数えないので 32 bit で足り、列の要素は今までの深さ（std::size_t）と同じ大きさのまま。
//
// round_end はその鍵が周の最後で、まだ周が残っている印（#1341）。列から取り出したとき、鍵を流す
// 前に replay_rounds_ の頂の周を列の頭へ積む（流した鍵が差し込む入れ子の再生はその前に来る）。
struct ReplayLevel
{
    std::uint32_t depth;
    core::VimReplayKind kind;
    bool round_end = false;
};
// 列の要素の大きさは今までの深さ（std::size_t）と同じ（表が空のときの費用を変えない）。
static_assert(sizeof(ReplayLevel) == sizeof(std::size_t));
} // namespace nenenib::application
