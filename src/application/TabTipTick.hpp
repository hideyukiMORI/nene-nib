#pragma once

namespace nenenib::application
{
// タブの札の待ちの合図（#1789・ADR 0056 の #1789 の追補）。マウスがタブに載ったとき ui が OS の
// 載せの待ち時間（SPI_GETMOUSEHOVERTIME）で 1 回のタイマーを張り、切れたらこれを 1 回送る。時刻は
// 運ばない。controller は待っていたタブにまだ載っていれば札を出す。AutosaveTick と同じく使う人の
// 操作ではないので、歩き・知らせ・変換・入力行・Vim の待ちの状態・記録・undo の単位を 1 つも動かさ
// ない（is_signal の 1 本に載る）。
struct TabTipTick
{
};
} // namespace nenenib::application
