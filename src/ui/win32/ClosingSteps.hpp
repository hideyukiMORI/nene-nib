#pragma once

#include "ClosingRoute.hpp"
#include "ClosingStep.hpp"

#include <array>
#include <span>
#include <utility>

namespace nenenib::ui::win32
{
// 閉じる手順の列（ADR 0133 の追補 #1340）。どの道も先に窓を隠してから書く。書き込みは HDD や
// ウイルス対策の下で 0.1〜12 秒かかるので、窓が消えるまでを書き込みの時間に依らせない。
// 書き込みは隠した後も UI スレッドで最後まで待つ（worker に移さない・待たずに抜けると失う）。
// OS の終了を止める理由は書き終えてから外し、本窓を壊すのは window_closes の最後の段だけ。
// 未保存の確認はこの列の前（end_window）で済んでいて、取り消せばこの列には入らない。
inline constexpr std::array window_closing_steps{
    ClosingStep::hide_window, ClosingStep::remember_session, ClosingStep::release_session_block,
    ClosingStep::destroy_window};
inline constexpr std::array session_ending_steps{ClosingStep::hide_window,
                                                 ClosingStep::remember_session};

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
} // namespace nenenib::ui::win32
