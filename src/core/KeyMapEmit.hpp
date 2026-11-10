#pragma once

#include "VimKey.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::core
{
// 表を引いた結果の「流す」（ADR 0176 の決定 2・4）。keys は engine へ順に流す鍵（右辺は生のまま・
// 不一致で流す左辺の分もここ）。waiting は流した後に残る次の待ちの鍵で、
// 不一致の後の鍵が別の左辺の途中なら空でない（決定 4「新しい待ちを始めうる」）。呼び手は
// 待ちの列を waiting に置き換える。ends は keys を区切る単位（表の 1 行の右辺か、表に無い 1 鍵）の
// 終わりの位置の昇順で、最後は keys の長さ。単位の途中の鍵が失敗したら呼び手はその単位の残りを
// 捨てて次の単位へ進む（Vim の beep_flush の FLUSH_MINIMAL は展開した鍵だけを捨て、打った鍵は
// 残す・#1257）。reads は ends と同じ並びで、単位ごとに引いた鍵の列（積んだ鍵と今の鍵、
// flush では積んだ鍵）のどこまでを読んだか。単位を流してモードが変わったら、呼び手は読んで
// いない残りを今のモードで引き直す（Vim の vgetorpeek は鍵ごとに今の State で表を引く・#1266）。
// remaps は ends と同じ並びで、単位ごとに右辺を 1 段引き直す鍵の始まりの位置（`map` の右辺の頭か、
// 右辺が左辺で始まるならその後・`noremap` の右辺と表に無い鍵は単位の終わり）。呼び手はそこから
// 単位の終わりまでを 1 鍵ごとに流す時点のモードで rhs_unit_step に引かせる（#1276）。
// mapped は ends と同じ並びで、単位が表の行に当たったか（偽は表に無い 1 鍵）。呼び手は当たった
// 単位の鍵を右辺の鍵（マクロに積まない）として流し、録画がその単位の前にも後にも立っていれば
// 打った左辺（reads の前の単位の終わりからこの単位まで）を録画に積む（#1312）。
struct KeyMapEmit
{
    std::vector<VimKey> keys;
    std::vector<VimKey> waiting;
    std::vector<std::size_t> ends;
    std::vector<std::size_t> reads;
    std::vector<std::size_t> remaps;
    std::vector<bool> mapped;
};
} // namespace nenenib::core
