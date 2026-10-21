#pragma once

namespace nenenib::application
{
// プレビューの間引きの合図（ADR 0155 の決定 8）。ui のタイマーが 200 ms ごとに送り、時刻は
// 運ばない。打つのを止めてから 1 tick 以上たった本文だけを要求する。AutosaveTick と同じく使う人の
// 操作ではないので、歩き・知らせ・変換・入力行・Vim の待ちの状態・記録・undo の単位を 1 つも
// 動かさない。
struct PreviewTick
{
};
} // namespace nenenib::application
