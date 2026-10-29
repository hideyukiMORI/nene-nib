#pragma once

#include "MapTimer.hpp"

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
// 鍵を積んだ間だけの 1 回）。増えたら下の switch が落ちる（CPP-002）。
enum class WindowTimer : UINT_PTR
{
    selection = 1,
    autosave = 2,
    preview = 3,
    language_server = 4,
    key_map = 5,
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
} // namespace nenenib::ui::win32
