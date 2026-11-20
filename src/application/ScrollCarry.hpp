#pragma once

#include "ScrollFraction.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// ホイールの送り（ScrollLines / ScrollPane の lines と parts）を行と残りに分けた値（#1898）。
// lines は送る画面行（正が下）。rest は端数を持つ送り先なら新しい端数（0 以上
// core::scroll_fraction_units 未満）、持たない送り先なら次へ繰り越す余り（向きの符号つき）。
// どちらの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct ScrollCarry
{
    std::int64_t lines;
    std::int64_t rest;
};

// 送りの量を 1/scroll_fraction_units 行の単位で数える。
[[nodiscard]] constexpr std::int64_t wheel_amount(std::int32_t lines, std::int32_t parts) noexcept
{
    return (static_cast<std::int64_t>(lines) *
            static_cast<std::int64_t>(core::scroll_fraction_units)) +
           parts;
}

// 端数を持つ送り先（text の表示の本文）。今の端数に量を足し、行の境を越えた分を画面行に
// 繰り上げる（下へも上へも床の割り算で、端数は常に 0 以上）。
[[nodiscard]] constexpr ScrollCarry fine_carried(std::size_t fraction, std::int32_t lines,
                                                 std::int32_t parts) noexcept
{
    const auto units = static_cast<std::int64_t>(core::scroll_fraction_units);
    const std::int64_t total = static_cast<std::int64_t>(fraction) + wheel_amount(lines, parts);
    const std::int64_t rest = ((total % units) + units) % units;
    return ScrollCarry{(total - rest) / units, rest};
}

// 端数を持たない送り先（格子・プレビュー）。余り remainder に量を足して行に変え、満たない分を
// 繰り越す。向きが反転したら前の余りを捨てる（ui の wheel_turned と同じ規則・#499）。
[[nodiscard]] constexpr ScrollCarry coarse_carried(std::int64_t remainder, std::int32_t lines,
                                                   std::int32_t parts) noexcept
{
    const auto units = static_cast<std::int64_t>(core::scroll_fraction_units);
    const std::int64_t amount = wheel_amount(lines, parts);
    const bool reversed = (remainder > 0 && amount < 0) || (remainder < 0 && amount > 0);
    const std::int64_t total = (reversed ? 0 : remainder) + amount;
    return ScrollCarry{total / units, total % units};
}
} // namespace nenenib::application
