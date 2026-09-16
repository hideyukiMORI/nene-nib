#pragma once

#include <cstdint>

namespace nenenib::core
{
// オペレータが取れる移動の閉じた一覧（ADR 0012 の決定 5）。d の範囲はこの値ごとに決まる。
enum class VimMotion : std::uint8_t
{
    left,
    down,
    up,
    right,
    line_start,
    line_end,
    next_word,
    previous_word
};
} // namespace nenenib::core
