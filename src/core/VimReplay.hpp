#pragma once

#include "VimKey.hpp"
#include "VimReplayKind.hpp"
#include "VimVisualExtent.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::core
{
// 再生の入れ子の深さの上限（ADR 0046 の決定 4）。再帰マクロの終端は失敗で作るのが慣用で、
// 100 段で足りる。超えたら controller が残りの鍵を捨てる。
inline constexpr std::size_t vim_replay_depth_limit = 100;

// いちばん外の再生（`.` `@` を打った 1 鍵）が列から流す鍵の数の上限（#1351・施主決定 D75）。
// 失敗しない中身の `999999999@a` は Vim と同じく回数ぶん回り窓が固まるので、超えたら controller
// が残りを捨てて 1 行知らせる。値は静かな機械の Release の nib_vim_replay で `x` や `j` の形の
// 100,000 鍵が約 1.5 秒で返る 10 のべき（ADR 0046 の追補 #1351 に測った数と理由）。
inline constexpr std::size_t vim_replay_key_limit = 100000;

// `.` の再生（ADR 0030 の決定 6 / ADR 0033 の決定 4）と `@` の再生（ADR 0046 の決定 3）。回数の
// 桁を先頭に展開した鍵の列と、VISUAL の記録なら選び直す範囲の大きさを運び、controller が選択を
// 置いてから同じ accept の経路へ鍵を 1 つずつ流す。kind が macro なら鍵は流す時点の `map` の表で
// 引く（#1312）。効果は 1 つのまま（VimSelect と 2 つ返さない）。本文は持たない。
//
// rounds は keys を流す周の数（`[count]@a` の回数・`.` は 1・#1341）。engine は鍵の列を回数ぶん
// 先に並べず、controller が 1 周を流し終えるたびに次の周を列の頭へ積み、鍵が失敗したら残りの周
// を捨てる（Vim の nv_at の回数のループと do_execreg の「失敗で残りを捨てる」）。並べる量は 1 周
// ぶんだけなので、`999999999@a` も確保は回数に比例しない。
struct VimReplay
{
    std::vector<VimKey> keys;
    std::optional<VimVisualExtent> reselect;
    VimReplayKind kind;
    std::size_t rounds;
    // INSERT の `Ctrl-o` の後の `@`（ADR 0182 の決定 7）。Vim の do_execreg は始め直す印を消して
    // `i` を再生の鍵の後ろへ積む（put_reedit_in_typebuf）ので、controller は全部の周の後に `i` を
    // 1 つ流す。再生が失敗して残りを捨てれば `i` も捨てる。
    bool insert_after = false;
};
} // namespace nenenib::core
