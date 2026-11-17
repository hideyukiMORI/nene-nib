#pragma once

#include <cstdint>

namespace nenenib::application
{
// ui に頼むタブの札の待ちのタイマー（#1789）。start は OS の載せの待ち時間・1 回を張る（張って
// いれば張り直して数え直す）、stop は外す。意図が待ちを変えなかったときは EditorDelivery の
// tab_tip_timer が空で、ui は何もしない。値が増えたら ui の switch が落ちる（CPP-002）。
enum class TabTipTimer : std::uint8_t
{
    start,
    stop
};
} // namespace nenenib::application
