#pragma once

#include "VimKey.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::core
{
// 表を引いた結果の「流す」（ADR 0176 の決定 2・4）。keys は engine へ順に流す鍵（右辺は表で
// 1 段だけ引き直し済み・不一致で流す左辺の分もここ）。waiting は流した後に残る次の待ちの鍵で、
// 不一致の後の鍵が別の左辺の途中なら空でない（決定 4「新しい待ちを始めうる」）。呼び手は
// 待ちの列を waiting に置き換える。ends は keys を区切る単位（表の 1 行の右辺か、表に無い 1 鍵）の
// 終わりの位置の昇順で、最後は keys の長さ。単位の途中の鍵が失敗したら呼び手はその単位の残りを
// 捨てて次の単位へ進む（Vim の beep_flush の FLUSH_MINIMAL は展開した鍵だけを捨て、打った鍵は
// 残す・#1257）。
struct KeyMapEmit
{
    std::vector<VimKey> keys;
    std::vector<VimKey> waiting;
    std::vector<std::size_t> ends;
};
} // namespace nenenib::core
