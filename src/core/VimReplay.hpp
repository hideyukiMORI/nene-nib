#pragma once

#include "VimKey.hpp"
#include "VimVisualExtent.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::core
{
// 再生の入れ子の深さの上限（ADR 0046 の決定 4）。再帰マクロの終端は失敗で作るのが慣用で、
// 100 段で足りる。超えたら controller が残りの鍵を捨てる。
inline constexpr std::size_t vim_replay_depth_limit = 100;

// `.` の再生（ADR 0030 の決定 6 / ADR 0033 の決定 4）。回数の桁を先頭に展開した鍵の列と、
// VISUAL の記録なら選び直す範囲の大きさを運び、controller が選択を置いてから同じ accept の
// 経路へ鍵を 1 つずつ流す。効果は 1 つのまま（VimSelect と 2 つ返さない）。本文は持たない。
struct VimReplay
{
    std::vector<VimKey> keys;
    std::optional<VimVisualExtent> reselect;
};
} // namespace nenenib::core
