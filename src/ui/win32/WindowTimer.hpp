#pragma once

#include <windows.h>

#include <optional>

namespace nenenib::ui::win32
{
// 窓が張るタイマーの閉じた集合。値は SetTimer の番号で、WM_TIMER の wParam に返る。selection は
// 画面外への選択を続ける 50 ms（押している間だけ）、autosave は自動退避の合図（ADR 0137 の決定 1・
// 窓の作成から壊れるまで）。増えたら下の switch が落ちる（CPP-002）。
enum class WindowTimer : UINT_PTR
{
    selection = 1,
    autosave = 2,
};

// 自動退避の合図の間隔（D47 の 30 秒）。時刻は読まず、OS のタイマーに数えさせる。
inline constexpr UINT autosave_interval_milliseconds = 30'000;

// WM_TIMER の wParam を窓のタイマーへ写す。窓が張っていない番号は値なし。
[[nodiscard]] constexpr std::optional<WindowTimer> window_timer_of(WPARAM id) noexcept
{
    const auto timer = static_cast<WindowTimer>(id);
    switch (timer)
    {
    case WindowTimer::selection:
    case WindowTimer::autosave:
        return timer;
    }
    return std::nullopt;
}
} // namespace nenenib::ui::win32
