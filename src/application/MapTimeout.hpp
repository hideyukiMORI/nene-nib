#pragma once

namespace nenenib::application
{
// Vim の `map` の待ちの合図（ADR 0176 の決定 4）。左辺の途中で鍵を積んだとき ui が 1 秒・1 回の
// タイマーを張り、切れたらこれを 1 回送る。時刻は運ばない。controller は積んだ鍵を
// key_map_flush で流して空にする。AutosaveTick と同じく使う人の操作ではないので、歩き・知らせを
// 動かさない（keeps_tab_walk と begin_intent の表に載る）。
struct MapTimeout
{
};
} // namespace nenenib::application
