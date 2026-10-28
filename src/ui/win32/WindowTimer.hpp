#pragma once

#include <windows.h>

#include <optional>

namespace nenenib::ui::win32
{
// 窓が張るタイマーの閉じた集合。値は SetTimer の番号で、WM_TIMER の wParam に返る。selection は
// 画面外への選択を続ける 50 ms（押している間だけ）、autosave は自動退避の合図（ADR 0137 の決定 1・
// 窓の作成から壊れるまで）、preview は Markdown のプレビューの解析を頼む合図（ADR 0155・frame に
// プレビューがある間だけ）、language_server は言語サーバーの古さの合図（ADR 0160 の決定 6・窓の
// 作成から壊れるまで）。増えたら下の switch が落ちる（CPP-002）。
enum class WindowTimer : UINT_PTR
{
    selection = 1,
    autosave = 2,
    preview = 3,
    language_server = 4,
};

// 自動退避の合図の間隔（D47 の 30 秒）。時刻は読まず、OS のタイマーに数えさせる。
inline constexpr UINT autosave_interval_milliseconds = 30'000;

// プレビューの合図の間隔（ADR 0155 の決定 8 の 200 ms）。時刻は読まず、OS のタイマーに数えさせる。
inline constexpr UINT preview_interval_milliseconds = 200;

// 言語サーバーの古さの合図の間隔（ADR 0160 の決定 6 の 30 秒）。時刻は読まず、OS のタイマーに
// 数えさせる。
inline constexpr UINT language_server_interval_milliseconds = 30'000;
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
        return timer;
    }
    return std::nullopt;
}
} // namespace nenenib::ui::win32
