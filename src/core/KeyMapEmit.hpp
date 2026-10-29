#pragma once

#include "VimKey.hpp"

#include <vector>

namespace nenenib::core
{
// 表を引いた結果の「流す」（ADR 0176 の決定 2・4）。keys は engine へ順に流す鍵（右辺は表で
// 1 段だけ引き直し済み・不一致で流す左辺の分もここ）。waiting は流した後に残る次の待ちの鍵で、
// 不一致の後の鍵が別の左辺の途中なら空でない（決定 4「新しい待ちを始めうる」）。呼び手は
// 待ちの列を waiting に置き換える。
struct KeyMapEmit
{
    std::vector<VimKey> keys;
    std::vector<VimKey> waiting;
};
} // namespace nenenib::core
