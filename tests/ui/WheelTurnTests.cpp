#include "EditorOperation.hpp"
#include "WheelTurn.hpp"
#include "WindowChecks.hpp"

#include <windows.h>

#include <cstdint>

namespace nenenib::tests::ui
{
namespace
{
// 正の余り → 負の delta の反転と、1 刻みの行数の上限の境界（#499 の差し戻し）。
void verify_wheel_edges()
{
    using nenenib::ui::win32::wheel_lines_limit;
    using nenenib::ui::win32::wheel_lines_per_notch;
    using nenenib::ui::win32::wheel_turned;
    const auto forward = wheel_turned(10, -10, 1);
    expect(forward.lines == 0 && forward.remainder == -10,
           "a positive remainder is discarded by a negative delta");

    constexpr std::int32_t largest_delta = 32'767;
    expect(wheel_lines_per_notch(10'000, 40) == wheel_lines_limit &&
               wheel_lines_per_notch(10'001, 40) == wheel_lines_limit,
           "the limit itself is kept and one more is held at the limit");
    const auto widest = wheel_turned(0, largest_delta, wheel_lines_limit);
    expect(widest.lines == -2'730'583 && widest.remainder == 40,
           "the largest delta at the limit stays inside std::int32_t");
}

// 面の一覧のホイール（#737）。窓の `scroll_palette` は本文と同じ `wheel_turned` に
// 面の余りと `wheel_lines_per_notch` の 1 刻みを渡し、`turn.lines` が 0 でなければ
// `ScrollPaletteList` を送る。nib_window_tests は窓を作らないので、窓が通す列を
// ここで同じ順に踏む。
void verify_palette_wheel_turns()
{
    using nenenib::ui::win32::wheel_lines_per_notch;
    using nenenib::ui::win32::wheel_turned;
    constexpr std::int32_t small_delta = 40;
    const auto one_line = wheel_lines_per_notch(1, 0);
    std::int32_t remainder = 0;
    std::int32_t sent = 0;
    std::int32_t messages = 0;
    for (int turn_index = 0; turn_index < 3; ++turn_index)
    {
        const auto turn = wheel_turned(remainder, -small_delta, one_line);
        remainder = turn.remainder;
        if (turn.lines != 0)
        {
            sent += turn.lines;
            ++messages;
        }
    }
    expect(sent == 1 && messages == 1 && remainder == 0,
           "three palette deltas of 40 accumulate into one ScrollPaletteList of 1 line down");

    const auto partial = wheel_turned(0, -small_delta, one_line);
    const auto back = wheel_turned(partial.remainder, small_delta, one_line);
    const auto back_again = wheel_turned(back.remainder, small_delta, one_line);
    expect(partial.lines == 0 && back.lines == 0 && back_again.lines == 0 &&
               back_again.remainder == 2 * small_delta,
           "turning the palette wheel back discards the remainder of the other direction");

    const auto notch = wheel_turned(0, WHEEL_DELTA, wheel_lines_per_notch(3, 0));
    expect(notch.lines == -3 && notch.remainder == 0,
           "one palette notch away scrolls the setting's 3 lines up");
}

// Ctrl つきのホイールは字の大きさ（#1048・ADR 0150 の追補）。窓の `zoom_wheel` は 1 刻みを
// 1 段にして wheel_turned に渡し、lines の負（奥）を font_larger、正（手前）を font_smaller の
// 回数にする。
void verify_zoom_wheel_turns()
{
    using nenenib::ui::win32::wheel_turned;
    constexpr std::int32_t one_step = 1;
    const auto larger = wheel_turned(0, WHEEL_DELTA, one_step);
    expect(larger.lines == -1 && larger.remainder == 0,
           "one Ctrl notch away makes the font one step larger");
    const auto half = wheel_turned(0, -WHEEL_DELTA / 2, one_step);
    const auto smaller = wheel_turned(half.remainder, -WHEEL_DELTA / 2, one_step);
    expect(half.lines == 0 && smaller.lines == 1 && smaller.remainder == 0,
           "a Ctrl notch towards the user makes the font one step smaller only when complete");
    using nenenib::core::EditorOperation;
    using nenenib::ui::win32::zoom_wheel_operation;
    expect(zoom_wheel_operation(larger.lines) == EditorOperation::font_larger &&
               zoom_wheel_operation(smaller.lines) == EditorOperation::font_smaller,
           "turning the Ctrl wheel away enlarges the font and towards the user shrinks it");
}
} // namespace

// 本文のホイールの余りと 1 刻みの行数（#499）。WM_MOUSEWHEEL の delta は奥が正で、ScrollLines の
// 正は下。窓は作らない（実機のホイールの手触りは hide の試用で確かめる）。
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
    verify_wheel_edges();
    verify_palette_wheel_turns();
    verify_zoom_wheel_turns();
}
} // namespace nenenib::tests::ui
