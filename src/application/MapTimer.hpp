#pragma once

#include <cstdint>

namespace nenenib::application
{
// ui に頼む `map` の待ちのタイマー（ADR 0176 の決定 4）。start は 1 秒・1 回を張る（張っていれば
// 張り直す）、stop は外す。意図が待ちを変えなかったときは EditorDelivery.map_timer が空で、ui は
// 何もしない。値が増えたら ui の switch が落ちる（CPP-002）。
enum class MapTimer : std::uint8_t
{
    start,
    stop
};
} // namespace nenenib::application
