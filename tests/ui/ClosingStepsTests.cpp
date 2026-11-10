// Issue #1340: 閉じる手順の列（ADR 0133
// の追補）。窓を閉じる道は変換を取り消して窓を隠してから書き、 本窓を壊すのは window_closes
// の最後の段だけ。WM_ENDSESSION は書くだけで隠さない。未保存の確認で
// 取り消せば列に入らない（save_step が stay を返し、end_window は leave を呼ばない）。
#include "ClosingRoute.hpp"
#include "ClosingStep.hpp"
#include "ClosingSteps.hpp"
#include "SaveAnswer.hpp"
#include "SaveQuestion.hpp"
#include "SaveStep.hpp"
#include "WindowChecks.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <span>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
using nenenib::ui::win32::closing_steps;
using nenenib::ui::win32::ClosingRoute;
using nenenib::ui::win32::ClosingStep;
using nenenib::ui::win32::run_closing_steps;
using nenenib::ui::win32::save_step;
using nenenib::ui::win32::SaveAnswer;
using nenenib::ui::win32::SaveQuestion;
using nenenib::ui::win32::SaveStep;

[[nodiscard]] std::ptrdiff_t position_of(std::span<const ClosingStep> steps, ClosingStep step)
{
    return std::distance(steps.begin(), std::ranges::find(steps, step));
}

[[nodiscard]] bool holds(std::span<const ClosingStep> steps, ClosingStep step)
{
    return std::ranges::find(steps, step) != steps.end();
}

// × と Alt+F4・最後のタブ・描画を失った終わり（abandon → end_window → leave）は同じ列。
void verify_window_closes()
{
    const auto steps = closing_steps(ClosingRoute::window_closes);
    constexpr std::array expected{ClosingStep::cancel_composition, ClosingStep::hide_window,
                                  ClosingStep::remember_session, ClosingStep::release_session_block,
                                  ClosingStep::destroy_window};
    expect(std::ranges::equal(steps, expected),
           "closing the window cancels the composition, hides, writes, releases the shutdown "
           "block, then destroys");
    expect(position_of(steps, ClosingStep::cancel_composition) <
               position_of(steps, ClosingStep::hide_window),
           "the composition is cancelled before hiding, so losing focus cannot commit it");
    expect(position_of(steps, ClosingStep::hide_window) <
               position_of(steps, ClosingStep::remember_session),
           "the window is hidden before the session, history and autosave are written");
    expect(position_of(steps, ClosingStep::remember_session) <
               position_of(steps, ClosingStep::release_session_block),
           "the shutdown block stays until the writes are done");
    expect(!steps.empty() && steps.back() == ClosingStep::destroy_window &&
               std::ranges::count(steps, ClosingStep::destroy_window) == 1,
           "the window is destroyed once, as the last step");
}

// WM_ENDSESSION（OS の終了が確定・WM_QUERYENDSESSION で通した後）は書くだけ。隠れている窓は
// 終了を止められず 5 秒で OS に終わらされるので、OS の終了の中では隠さない。窓は壊さない
// （ADR 0059 の決定 3）。
void verify_session_ends()
{
    const auto steps = closing_steps(ClosingRoute::session_ends);
    constexpr std::array expected{ClosingStep::remember_session};
    expect(std::ranges::equal(steps, expected), "an ending session only writes the session");
    expect(!holds(steps, ClosingStep::hide_window),
           "an ending session does not hide the window (a hidden window is ended after 5 seconds)");
    expect(!holds(steps, ClosingStep::destroy_window),
           "an ending session does not destroy the window");
}

// 段を写す口は run_closing_steps の 1 本。替え玉で呼ばれた段を数え、列のとおりに 1 回ずつ呼ぶ。
[[nodiscard]] std::vector<ClosingStep> steps_run(ClosingRoute route)
{
    std::vector<ClosingStep> calls;
    run_closing_steps(route, [&calls](ClosingStep step) { calls.push_back(step); });
    return calls;
}

void verify_runner_follows_steps()
{
    const auto closes = steps_run(ClosingRoute::window_closes);
    expect(std::ranges::equal(closes, closing_steps(ClosingRoute::window_closes)),
           "closing the window runs every step of its list once, in order");
    expect(std::ranges::count(closes, ClosingStep::destroy_window) == 1 &&
               std::ranges::count(closes, ClosingStep::hide_window) == 1,
           "closing the window hides once and destroys once");
    const auto ends = steps_run(ClosingRoute::session_ends);
    expect(std::ranges::equal(ends, closing_steps(ClosingRoute::session_ends)),
           "an ending session runs its list once, in order");
    expect(std::ranges::count(ends, ClosingStep::hide_window) == 0 &&
               std::ranges::count(ends, ClosingStep::destroy_window) == 0,
           "an ending session neither hides nor destroys the window");
}

// 取り消しは閉じる列に入らない。閉じる確認のキャンセル（と箱を出せなかったとき）は stay で、
// end_window は leave を呼ばずに戻る。描画を失った後は取消が無いので閉じる側へ。
void verify_cancel_stays_visible()
{
    expect(save_step(SaveQuestion::unsaved, SaveAnswer::cancel) == SaveStep::stay,
           "cancelling the unsaved question stays, so the window is not hidden");
    expect(save_step(SaveQuestion::rendering_lost, SaveAnswer::cancel) == SaveStep::close_keeping,
           "after rendering is lost there is no cancel, so the window closes keeping the autosave");
}
} // namespace

void verify_closing_steps()
{
    verify_window_closes();
    verify_session_ends();
    verify_runner_follows_steps();
    verify_cancel_stays_visible();
}
} // namespace nenenib::tests::ui
