#include "WindowChecks.hpp"
#include "WindowTimer.hpp"

#include <utility>

namespace nenenib::tests::ui
{
// 窓のタイマーの番号の写し（ADR 0137 の決定 1）。WM_TIMER の autosave の番号は自動退避の合図だけに
// 写り、選択の番号とも窓が張っていない番号とも混ざらない。窓は作らない（実機の WM_TIMER は
// eng/verify-window.py --autosave が送る）。
void verify_window_timers()
{
    using nenenib::ui::win32::window_timer_of;
    using nenenib::ui::win32::WindowTimer;
    constexpr WPARAM autosave = std::to_underlying(WindowTimer::autosave);
    constexpr WPARAM selection = std::to_underlying(WindowTimer::selection);
    expect(autosave != selection, "the autosave timer has its own number");
    expect(window_timer_of(autosave) == WindowTimer::autosave,
           "WM_TIMER with the autosave number is the autosave tick");
    expect(window_timer_of(selection) == WindowTimer::selection,
           "WM_TIMER with the selection number keeps scrolling the selection");
    constexpr WPARAM preview = std::to_underlying(WindowTimer::preview);
    expect(preview != autosave && preview != selection, "the preview timer has its own number");
    expect(window_timer_of(preview) == WindowTimer::preview,
           "WM_TIMER with the preview number is the preview tick");
    expect(!window_timer_of(0).has_value() && !window_timer_of(4).has_value(),
           "a number the window never set is ignored");
    expect(nenenib::ui::win32::preview_interval_milliseconds == 200,
           "the preview tick comes every 200 ms (ADR 0155)");
    expect(nenenib::ui::win32::autosave_interval_milliseconds == 30'000,
           "the autosave tick comes every 30 seconds (D47)");
}
} // namespace nenenib::tests::ui
