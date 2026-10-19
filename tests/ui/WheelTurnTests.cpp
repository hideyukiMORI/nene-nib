#include "WheelTurn.hpp"
#include "WindowChecks.hpp"

#include <windows.h>

#include <cstdint>

namespace nenenib::tests::ui
{
// 本文のホイールの余りと 1 刻みの行数（#499）。WM_MOUSEWHEEL の delta は奥が正で、ScrollLines の
// 正は下。窓は作らない（実機のホイールは eng/verify-window.py が WHEEL_DELTA 単位で送る）。
void verify_wheel_turns()
{
    using nenenib::ui::win32::wheel_lines_per_notch;
    using nenenib::ui::win32::wheel_turned;
    constexpr std::int32_t three = 3;
    expect(wheel_lines_per_notch(3, 40) == 3, "the OS setting gives the lines per notch");
    expect(wheel_lines_per_notch(WHEEL_PAGESCROLL, 40) == 40,
           "WHEEL_PAGESCROLL scrolls one screen per notch");
    expect(wheel_lines_per_notch(0, 40) == 0, "a zero setting does not scroll");
    expect(wheel_lines_per_notch(4'000'000'000U, 40) == nenenib::ui::win32::wheel_lines_limit,
           "a huge setting is held under the limit");

    const auto notch = wheel_turned(0, -WHEEL_DELTA, three);
    expect(notch.lines == 3 && notch.remainder == 0, "one notch towards the user is 3 lines down");
    const auto away = wheel_turned(0, WHEEL_DELTA, three);
    expect(away.lines == -3 && away.remainder == 0, "one notch away is 3 lines up");

    const auto first = wheel_turned(0, -60, three);
    expect(first.lines == 1 && first.remainder == -60, "half a notch is carried after 1 line");
    const auto second = wheel_turned(first.remainder, -60, three);
    expect(second.lines == 2 && second.remainder == 0, "two deltas of 60 make 3 lines in all");

    const auto small = wheel_turned(0, -10, 1);
    expect(small.lines == 0 && small.remainder == -10, "a small touchpad delta is accumulated");
    const auto reversed = wheel_turned(small.remainder, 10, 1);
    expect(reversed.lines == 0 && reversed.remainder == 10,
           "turning back discards the remainder of the other direction");

    const auto single = wheel_turned(0, WHEEL_DELTA, 1);
    expect(single.lines == -1 && single.remainder == 0, "a 1-line setting turns 120 into 1 line");
    const auto still = wheel_turned(0, WHEEL_DELTA, 0);
    expect(still.lines == 0 && still.remainder == 0, "a zero setting carries nothing");
}
} // namespace nenenib::tests::ui
