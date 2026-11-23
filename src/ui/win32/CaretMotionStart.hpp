#pragma once

#include "CaretMotionClock.hpp"
#include "CaretQuad.hpp"
#include "PaneSide.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::ui::win32
{
// キャレットの動きの frame の入口で描く側が渡す値（ADR 0192 の決定 3・5・6）。wanted は設定が
// オンで IME の変換中でないか、tab は操作中のタブの位置（変われば跳ぶ）、progress は controller が
// frame に載せた最後の合図の進み、duration は動きの時間（ms）、bounds は本文の領域（左上が
// corners[0]・右下が corners[2]・操作中の枠の側が同じまま変われば跳ぶ）、side は操作中の枠の側
// （変わった frame は領域が変わっても動く・決定 5）。公開 aggregate（ADR 0007）。
struct CaretMotionStart final
{
    bool wanted = false;
    std::size_t tab = 0;
    core::CaretMotionClock progress{};
    std::uint32_t duration = 0;
    core::CaretQuad bounds{};
    core::PaneSide side = core::PaneSide::first;
};
} // namespace nenenib::ui::win32
