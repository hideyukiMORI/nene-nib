#pragma once

#include "Offset.hpp"
#include "VimRepeatFailure.hpp"

#include <optional>

namespace nenenib::core
{
// 移動の着地（Issue #224）。offset は移動が行ける所まで動いた位置、failure は Vim の移動が
// FAIL を返したときの失敗の印。後ろ向きの語の移動（b B ge gE）は回数の途中で本文の先頭に
// 当たると、着地まで動いたうえで失敗する（Vim の bck_word / bckend_word）。裸の移動は着地へ
// 動いてビープし、オペレータは打ち消される（clearopbeep）。どの値も単独で妥当な公開 aggregate。
struct VimMotionLanding
{
    Offset offset;
    std::optional<VimRepeatFailure> failure = std::nullopt;
};
} // namespace nenenib::core
