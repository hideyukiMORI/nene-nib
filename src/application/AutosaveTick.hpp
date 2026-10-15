#pragma once

namespace nenenib::application
{
// 自動退避の合図（ADR 0137 の決定 1）。ui のタイマーが 30 秒ごとに送り、時刻は運ばない。何を
// 退避するかは controller が autosave_plan で決める。WorkCompleted と同じく使う人の操作ではない
// ので、歩き・知らせ・変換・入力行・Vim の待ちの状態・記録・undo の単位を 1 つも動かさない。
struct AutosaveTick
{
};
} // namespace nenenib::application
