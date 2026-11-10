#pragma once

#include "ClosingRoute.hpp"
#include "ClosingStep.hpp"

#include <array>
#include <span>
#include <utility>

namespace nenenib::ui::win32
{
// 閉じる手順の列（ADR 0133 の追補 #1340）。窓を閉じる道は先に窓を隠してから書く。書き込みは HDD
// やウイルス対策の下で 0.1〜12 秒かかるので、窓が消えるまでを書き込みの時間に依らせない。
// 隠す前に IME の変換を取り消す（隠すと届く WM_KILLFOCUS で確定して本文が動かないように）。
// 書き込みは隠した後も UI スレッドで最後まで待つ（worker に移さない・待たずに抜けると失う）。
// OS の終了を止める理由は書き終えてから外し、本窓を壊すのは window_closes の最後の段だけ。
// 未保存の確認はこの列の前（end_window）で済んでいて、取り消せばこの列には入らない。
// OS の終了が確定した WM_ENDSESSION は書くだけで隠さない。見える窓の無いアプリは終了を止められず、
// 5 秒で答えなければ OS に終わらされる（Shutdown Changes for Windows Vista）。終了の画面に
// 替わっているので隠しても得は無く、書く時間が縮むだけ。
inline constexpr std::array window_closing_steps{
    ClosingStep::cancel_composition, ClosingStep::hide_window, ClosingStep::remember_session,
    ClosingStep::release_session_block, ClosingStep::destroy_window};
inline constexpr std::array session_ending_steps{ClosingStep::remember_session};

[[nodiscard]] constexpr std::span<const ClosingStep> closing_steps(ClosingRoute route) noexcept
{
    switch (route)
    {
    case ClosingRoute::window_closes:
        return window_closing_steps;
    case ClosingRoute::session_ends:
        return session_ending_steps;
    }
    std::unreachable();
}

// 列を受けて順に段を呼ぶ唯一の口。EditorWindow::close_through（leave と WM_ENDSESSION）が段の写しを
// 渡し、試験は替え玉で呼ばれた段を数える。
template <typename Runner> constexpr void run_closing_steps(ClosingRoute route, const Runner &run)
{
    for (const ClosingStep step : closing_steps(route))
    {
        run(step);
    }
}
} // namespace nenenib::ui::win32
