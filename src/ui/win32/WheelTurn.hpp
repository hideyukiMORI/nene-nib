#pragma once

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace nenenib::ui::win32
{
// 本文のホイールの 1 回の結果（#499）。lines の正が下（ScrollLines と同じ・ADR 0009 の決定 6）。
// remainder は「delta × 1 刻みの行数」の単位で、WHEEL_DELTA に満たない分を次へ繰り越す。
struct WheelTurn
{
    std::int32_t remainder;
    std::int32_t lines;
};

// OS の SPI_GETWHEELSCROLLLINES が読めないときの既定（Windows の既定と同じ 3 行）。
inline constexpr UINT default_wheel_scroll_lines = 3;

// 1 刻みの行数の上限。delta（最大 32767）との積が std::int32_t に収まる大きさに抑える。
inline constexpr std::int32_t wheel_lines_limit = 10'000;

// SPI_GETWHEELSCROLLLINES の値から 1 刻みの行数を決める。WHEEL_PAGESCROLL は 1 刻みで 1 画面
// （見えている行数）。0 は OS の意味のまま「回しても動かない」。
[[nodiscard]] constexpr std::int32_t wheel_lines_per_notch(UINT setting,
                                                           std::size_t visible_lines) noexcept
{
    if (setting == WHEEL_PAGESCROLL)
    {
        return static_cast<std::int32_t>(
            std::min(visible_lines, static_cast<std::size_t>(wheel_lines_limit)));
    }
    return static_cast<std::int32_t>(std::min(setting, static_cast<UINT>(wheel_lines_limit)));
}

// 余りに delta を足して行数に変える。高分解能のホイールとタッチパッドの小さな delta は累積して
// から行になる。向きが反転したら前の余りを捨てる。per_notch は wheel_lines_per_notch の値で、
// delta は奥が正（WM_MOUSEWHEEL のまま）。
[[nodiscard]] constexpr WheelTurn wheel_turned(std::int32_t remainder, std::int32_t delta,
                                               std::int32_t per_notch) noexcept
{
    const bool reversed = (remainder > 0 && delta < 0) || (remainder < 0 && delta > 0);
    const std::int32_t scaled = (reversed ? 0 : remainder) + (delta * per_notch);
    return WheelTurn{scaled % WHEEL_DELTA, -(scaled / WHEEL_DELTA)};
}
} // namespace nenenib::ui::win32
