#pragma once

#include "VimKey.hpp"
#include "VimReplayKind.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nenenib::application
{
// `[count]@a` のまだ積んでいない周（#1341・ADR 0046 の決定 3 の追補）。controller は 1 周ぶんの
// 鍵だけを列に置き、周の最後の鍵（ReplayLevel の round_end）を取り出したときに次の周を列の頭へ
// 積む。remaining はまだ積んでいない周の数で、0 になった周はその時点で棄てる。入れ子の再生は
// 積み重ねで、頂がいま流している最も深い周。鍵の失敗と深さの上限で列と一緒に全部棄てる。
struct ReplayRounds
{
    std::vector<core::VimKey> keys;
    std::size_t remaining;
    std::uint32_t depth;
    core::VimReplayKind kind;
};
} // namespace nenenib::application
