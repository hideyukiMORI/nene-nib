#pragma once

#include <cstdint>

namespace nenenib::core
{
// 鍵で枠の大きさを変える仕方（ADR 0148 の 2026-11-22 の追補・#1941）。どれも操作中の枠の大きさを
// 決め、もう 1 つの枠が残りを取る（本物の Vim の実測と同じ）。grow / shrink は升の数だけ足し引き、
// set は升の数ちょうど（0 は最小）、maximize はもう 1 つの枠を最小にする、even は等分に戻す。
// 値を足したら落ちた switch を直す（CPP-002）。
enum class PaneResize : std::uint8_t
{
    grow,
    shrink,
    set,
    maximize,
    even
};
} // namespace nenenib::core
