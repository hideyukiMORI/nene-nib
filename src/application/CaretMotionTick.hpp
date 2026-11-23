#pragma once

#include "CaretMotionClock.hpp"

namespace nenenib::application
{
// キャレットの動きの合図（ADR 0192 の決定 3・#1933）。キャレットが動いている間だけ ui が窓のタイマ
// ーで送り、動きの番号とその動きが始まってからの経過（ms・ui が時計の port で測る）を運ぶ。
// controller は値を覚えて frame に載せるだけで、状態を何も変えない。AutosaveTick と同じく使う人の操
// 作ではないので、歩き・知らせ・変換・入力行・Vim の待ちの状態・記録・undo の単位を 1 つも動かさな
// い（is_signal の 1 本に載り、keeps_tab_walk と begin_intent は not_user_operation を経て合図の側
// に数える）。sparks は打ったときの粒の時計の番号と経過（ADR 0192 の追補・#1979）で、粒が生きている
// 間も同じタイマーが運ぶ（動きと粒のどちらかが生きている間だけ送る）。
struct CaretMotionTick
{
    core::CaretMotionClock progress{};
    core::CaretMotionClock sparks{};
};
} // namespace nenenib::application
