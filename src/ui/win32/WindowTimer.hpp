#pragma once

#include "MapTimer.hpp"
#include "TabTipTimer.hpp"

#include <windows.h>

#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
// 窓が張るタイマーの閉じた集合。値は SetTimer の番号で、WM_TIMER の wParam に返る。selection は
// 画面外への選択を続ける 50 ms（押している間だけ）、autosave は自動退避の合図（ADR 0137 の決定 1・
// 窓の作成から壊れるまで）、preview は Markdown のプレビューの解析を頼む合図（ADR 0155・frame に
// プレビューがある間だけ）、language_server は言語サーバーの古さの合図（ADR 0160 の決定 6・窓の
// 作成から壊れるまで）、key_map は Vim の `map` の待ち（ADR 0176 の決定 4・左辺の途中で
// 鍵を積んだ間だけの 1 回）、tab_tip はタブの札の待ち（#1789・タブに載せた間だけの 1 回）、
// caret_motion はキャレットの動きの合図（ADR 0192 の決定 3・動いている間だけ）、settings_choice は
// 設定の選ぶ欄の一覧の頭文字の 1 秒（#1926・一覧へ字を送るたびに張り直す 1 回）。増えたら下の
// switch が落ちる（CPP-002）。
enum class WindowTimer : UINT_PTR
{
    selection = 1,
    autosave = 2,
    preview = 3,
    language_server = 4,
    key_map = 5,
    tab_tip = 6,
    caret_motion = 7,
    settings_choice = 8,
};

// 自動退避の合図の間隔（D47 の 30 秒）。時刻は読まず、OS のタイマーに数えさせる。
inline constexpr UINT autosave_interval_milliseconds = 30'000;

// プレビューの合図の間隔（ADR 0155 の決定 8 の 200 ms）。時刻は読まず、OS のタイマーに数えさせる。
inline constexpr UINT preview_interval_milliseconds = 200;

// 言語サーバーの古さの合図の間隔（ADR 0160 の決定 6 の 30 秒）。時刻は読まず、OS のタイマーに
// 数えさせる。
inline constexpr UINT language_server_interval_milliseconds = 30'000;

// `map` の待ちの長さ（ADR 0176 の決定 4 の 1 秒・Vim の timeoutlen の既定）。時刻は読まず、OS の
// タイマーに数えさせる。
inline constexpr UINT key_map_timeout_milliseconds = 1'000;

// 選ぶ欄の一覧の頭文字の跳びで、打った字を忘れるまでの間（#1926・採用案の 1 秒）。時刻は読まず、
// OS のタイマーに数えさせる。
inline constexpr UINT settings_choice_typing_milliseconds = 1'000;

// タブの札の待ち（#1789）の既定。OS の載せの待ち時間（SPI_GETMOUSEHOVERTIME）が読めないときの
// Windows の既定の 400 ms。
inline constexpr UINT default_tab_tip_milliseconds = 400;

// キャレットの動きの合図の間隔（ADR 0192 の決定 3）。OS のタイマーの最小（USER_TIMER_MINIMUM）の
// 10 ms。経過は合図の間隔で数えず、時計の port で測る。
inline constexpr UINT caret_motion_interval_milliseconds = 10;

// WM_TIMER の wParam を窓のタイマーへ写す。窓が張っていない番号は値なし。
[[nodiscard]] constexpr std::optional<WindowTimer> window_timer_of(WPARAM id) noexcept
{
    const auto timer = static_cast<WindowTimer>(id);
    switch (timer)
    {
    case WindowTimer::selection:
    case WindowTimer::autosave:
    case WindowTimer::preview:
    case WindowTimer::language_server:
    case WindowTimer::key_map:
    case WindowTimer::tab_tip:
    case WindowTimer::caret_motion:
    case WindowTimer::settings_choice:
        return timer;
    }
    return std::nullopt;
}

// controller が頼んだ `map` の待ちのタイマーを張る・外す（決定 4）。start は張り直しで 1 秒を
// 数え直す。頼みが無ければ何もしない。
inline void follow_key_map_timer(HWND window, std::optional<application::MapTimer> request) noexcept
{
    if (!request.has_value())
    {
        return;
    }
    switch (request.value())
    {
    case application::MapTimer::start:
        static_cast<void>(SetTimer(window, std::to_underlying(WindowTimer::key_map),
                                   key_map_timeout_milliseconds, nullptr));
        return;
    case application::MapTimer::stop:
        static_cast<void>(KillTimer(window, std::to_underlying(WindowTimer::key_map)));
        return;
    }
}

// controller が頼んだタブの札の待ちのタイマーを張る・外す（#1789）。start は張り直しで待ちを
// 数え直す。待ちの長さは窓が起動と WM_SETTINGCHANGE で読んだ OS の載せの待ち時間。頼みが無ければ
// 何もしない。
inline void follow_tab_tip_timer(HWND window, std::optional<application::TabTipTimer> request,
                                 UINT milliseconds) noexcept
{
    if (!request.has_value())
    {
        return;
    }
    switch (request.value())
    {
    case application::TabTipTimer::start:
        static_cast<void>(
            SetTimer(window, std::to_underlying(WindowTimer::tab_tip), milliseconds, nullptr));
        return;
    case application::TabTipTimer::stop:
        static_cast<void>(KillTimer(window, std::to_underlying(WindowTimer::tab_tip)));
        return;
    }
}
} // namespace nenenib::ui::win32
