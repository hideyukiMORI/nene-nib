#pragma once

#include <cstdint>

namespace nenenib::application
{
// 本文を続けて押した回数（#1250・ADR 0056 の決定 9 の追補）。1 回目はキャレットを置き、2
// 回目は指した 語（Vim の `iw` と同じ範囲）、3 回目以降は行を選ぶ。数えるのは ui（OS
// の押しの時刻と位置）で、 application は回数だけを受ける。
enum class PointerClicks : std::uint8_t
{
    single,
    double_,
    triple
};
} // namespace nenenib::application
