#pragma once

#include "CaretMotionClock.hpp"

namespace nenenib::application
{
// キャレットの動きの合図（ADR 0192 の決定 3・#1933）。キャレットが動いている間だけ ui が窓の
// タイマーで送り、動きの番号とその動きが始まってからの経過（ms・ui が時計の port で測る）を運ぶ。
// controller は値を覚えて frame に載せるだけで、状態を何も変えない。AutosaveTick と同じく使う人の
// 操作ではないので、歩き・知らせ・変換・入力行・Vim の待ちの状態・記録・undo の単位を 1 つも
// 動かさない（is_signal の 1 本に載り、keeps_tab_walk と begin_intent は not_user_operation を経て
// 合図の側に数える）。
struct CaretMotionTick
{
    core::CaretMotionClock progress{};
};
} // namespace nenenib::application
