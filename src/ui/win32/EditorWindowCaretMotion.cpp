// キャレットの動きの合図（ADR 0192 の決定 3・6・#1933）。窓は描いた後に renderer が動いているかを
// 読み、動いている間だけ合図のタイマーを張って経過を CaretMotionTick で送る。四隅は core の純関数が
// 決め、窓は時計の port で経過を測るだけ（ARC-007）。
#include "EditorWindow.hpp"

#include "CaretMotionTick.hpp"
#include "WindowTimer.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace nenenib::ui::win32
{
void EditorWindow::read_animation_setting()
{
    BOOL animated = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animated, 0) == FALSE)
    {
        animated = TRUE;
    }
    animation_setting_ = animated != FALSE;
}

void EditorWindow::follow_caret_motion()
{
    const auto moving = renderer_->caret_motion_generation();
    if (!moving.has_value())
    {
        stop_caret_motion();
        return;
    }
    if (moving.value() != caret_motion_generation_ || !caret_motion_timing_)
    {
        caret_motion_generation_ = moving.value();
        caret_motion_start_ = clock_.milliseconds();
    }
    if (!std::exchange(caret_motion_timing_, true))
    {
        static_cast<void>(SetTimer(window_, std::to_underlying(WindowTimer::caret_motion),
                                   caret_motion_interval_milliseconds, nullptr));
    }
}

void EditorWindow::stop_caret_motion()
{
    if (std::exchange(caret_motion_timing_, false))
    {
        static_cast<void>(KillTimer(window_, std::to_underlying(WindowTimer::caret_motion)));
    }
}

// 最小化した・隠れた窓には WM_PAINT が来ないことがあり、描いた後の follow_caret_motion だけでは
// タイマーが外れない（#1956 の R1）。見えていなければ合図を送らずにタイマーを外し、renderer の
// 覚えを捨てる（次に見えた frame は跳ぶ）。
void EditorWindow::send_caret_motion()
{
    if (!caret_motion_timing_)
    {
        return;
    }
    if (IsIconic(window_) != FALSE || IsWindowVisible(window_) == FALSE)
    {
        stop_caret_motion();
        if (renderer_ != nullptr)
        {
            renderer_->settle_caret_motion();
        }
        return;
    }
    const std::uint64_t now = clock_.milliseconds();
    const std::uint64_t elapsed = now >= caret_motion_start_ ? now - caret_motion_start_ : 0;
    const auto clamped = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(elapsed, std::numeric_limits<std::uint32_t>::max()));
    send(application::CaretMotionTick{{caret_motion_generation_, clamped}});
}
} // namespace nenenib::ui::win32
