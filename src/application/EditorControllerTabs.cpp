#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "DevicePixels.hpp"
#include "DisplayInquiry.hpp"
#include "DisplayText.hpp"
#include "Edit.hpp"
#include "ExWindowCommand.hpp"
#include "PaneView.hpp"
#include "PreviewView.hpp"
#include "SplitLayout.hpp"
#include "TabReach.hpp"
#include "TabRecency.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "VisualPosition.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>
#include <variant>

namespace nenenib::application
{
using detail::parked_path;
using detail::scroll_extent_for;

namespace
{
// 帯の位置 position のタブのパス（無題なら無し）。脇の束は帯の位置からアクティブを抜いた順。
[[nodiscard]] std::optional<core::FilePath> tab_path_at(const EditorState &state,
                                                        std::size_t position)
{
    if (position == state.active_tab())
    {
        return state.document().path;
    }
    const std::size_t parked = position < state.active_tab() ? position : position - 1;
    return std::visit([](const auto &value) { return std::optional{parked_path(value)}; },
                      state.parked().at(parked));
}
} // namespace

void EditorController::adjust_other(const core::Edit &edit, const core::TextBuffer &after)
{
    // 1 枠では状態を写さない（1 打鍵の道に写しを足さない）。本文を置く前に呼ぶ（先頭行の行頭は
    // 編集の前の本文で引く）。
    if (state_.split().other.has_value())
    {
        state_ = state_.with_other_adjusted(edit, after);
    }
}

// 2 枠で操作中の枠を閉じる（ADR 0148 の決定 5）。もう 1 つの枠の表示を持ってきてから 1 枠へ。
// プレビューの枠へは移らないので、プレビューだけを閉じる（本文は閉じない・ADR 0155 の決定 4）。
void EditorController::close_operated_pane()
{
    if (state_.preview() == nullptr)
    {
        accept(SwitchPane{});
    }
    state_ = state_.without_split();
}

// `:only` はもう 1 つの枠を閉じる（ADR 0148 の決定 5）。
void EditorController::run_window_command(core::ExWindowCommand command)
{
    switch (command)
    {
    case core::ExWindowCommand::only:
        accept(ClosePane{});
        return;
    }
    std::unreachable();
}

void EditorController::accept(const NewTab &)
{
    leave_document();
    state_ = state_.with_new_tab();
    enter_document();
}

void EditorController::accept(const SwitchTab &intent)
{
    if (intent.index >= state_.tab_count())
    {
        return;
    }
    // アクティブ自身は文書を動かさず、歩きの途中なら着いた所で確定する（ADR 0058 の決定 2）。
    if (intent.index == state_.active_tab())
    {
        state_ = state_.with_switched(intent.index);
        return;
    }
    // 行き先がまだ読んでいない文書ならここで読む。読めなければそのタブは外れて知らせだけが残り、
    // 今の文書の一時の値（入力行・変換中の文字列）は閉じない（ADR 0059 の決定 5）。
    if (reach_tab(intent.index) != TabReach::reached)
    {
        return;
    }
    leave_document();
    state_ = state_.with_switched(intent.index);
    enter_document();
}

void EditorController::accept(const WalkRecentTab &intent)
{
    const auto destination =
        core::tab_recency_walked(state_.recency(), state_.active_tab(), intent.step);
    if (!destination.has_value())
    {
        return;
    }
    // 1 本のときの歩きは同じ位置で、文書は動かさず歩いている印だけを立てる。
    if (destination.value() == state_.active_tab())
    {
        state_ = state_.with_walked(destination.value());
        return;
    }
    // 行き先が読めなければ外れて、歩きは今のタブのまま続く。次の 1 歩は外れた後の使った順の
    // 隣へ行く（ADR 0059 の決定 5）。
    if (reach_tab(destination.value()) != TabReach::reached)
    {
        return;
    }
    leave_document();
    state_ = state_.with_walked(destination.value());
    enter_document();
}

void EditorController::accept(const SettleRecentTab &)
{
    state_ = state_.with_walk_settled();
}

void EditorController::accept(const CloseTab &intent)
{
    if (intent.index >= state_.tab_count())
    {
        return;
    }
    // 最後の 1 つは状態を変えず、窓を閉じる印だけを立てる（D22）。
    if (state_.tab_count() == 1)
    {
        state_ = state_.with_closing(true);
        return;
    }
    // 閉じたタブに載せていたマウスは、そのタブと一緒に消える（ADR 0056 の決定 2）。
    const auto &hovered = state_.hovered();
    if (hovered.has_value() && hovered.value().tab == intent.index &&
        (hovered.value().hit == core::TitleBarHit::tab ||
         hovered.value().hit == core::TitleBarHit::tab_close))
    {
        state_ = state_.with_hovered(std::nullopt);
    }
    // 閉じるタブにパスがあれば、閉じた後に履歴へ記録する（ADR 0060 の決定 8）。最後の 1 本は上で
    // 窓を閉じる印だけを立て、窓が閉じるとき（EndSession）に記録する。隣が読めずに外れたタブは
    // 記録しない（使う人が閉じたのは 1 本だけ）。
    const std::optional<core::FilePath> closed = tab_path_at(state_, intent.index);
    // 脇のタブを閉じてもアクティブな文書は動かないので、一時の値も閉じない。
    if (intent.index != state_.active_tab())
    {
        state_ = state_.with_closed(intent.index);
        reveal_active_tab();
    }
    else
    {
        close_active_tab();
    }
    if (closed.has_value())
    {
        remember({closed.value()});
    }
}

// ---------------------------------------------------------------- 枠（ADR 0148）

// 分けるのは同じ文書の 2 表示なので、読むものは無い（決定 2）。もう 1 つの枠は解決済みの
// スクロールを写す。
void EditorController::accept(const SplitPane &intent)
{
    // プレビューを捨てて本文の枠を置く（ADR 0155 の決定 4）。
    if (state_.preview() != nullptr)
    {
        state_ = state_.without_split();
    }
    if (state_.split().layout != core::SplitLayout::none)
    {
        state_ =
            state_.with_command_message(core::DisplayText::parse("もう分かれています").value());
        return;
    }
    settle_follow();
    state_ = state_.with_split(intent.layout);
}

// 枠を移る（決定 3・10）。違う文書ならタブの切り替えと同じく先に読む（読み込み済みのはず・
// 決定 2）。同じ文書でも入力行・変換・追従は移る前に閉じ、着いた枠のスクロールを収める
// （leave_document / enter_document の 1 対をそのまま通す）。
void EditorController::accept(const SwitchPane &)
{
    // プレビューの枠は操作中にならない（ADR 0155 の決定 4）。鳴らさず 1 行だけ知らせる。
    if (state_.preview() != nullptr)
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("プレビューの枠には入れません").value());
        return;
    }
    const PaneView *other = state_.other_pane();
    if (other == nullptr)
    {
        return;
    }
    const auto position = tab_position_of(other->tab);
    const TabReach reach = position.has_value() ? reach_tab(position.value()) : TabReach::dropped;
    switch (reach)
    {
    case TabReach::reached:
        break;
    case TabReach::reading:
        // 裏で読んでいる間は枠をそのまま残す（揃ったらそのタブへ切り替わる・ADR 0159 の追補 1）。
        return;
    case TabReach::dropped:
        // 読めなかった文書は reach_tab が帯から外して知らせる。もう 1 つの枠も外す。
        state_ = state_.without_split();
        return;
    }
    leave_document();
    state_ = state_.with_pane_swapped();
    enter_document();
}

void EditorController::accept(const ClosePane &)
{
    state_ = state_.without_split();
}

// もう 1 つの枠だけを送る（決定 11）。送り方と収め方は ScrollLines と同じ 2 本（shifted と
// constrained）を、もう 1 つの枠を操作中の枠の形で読む写しに通す。
void EditorController::accept(const ScrollPane &intent)
{
    // プレビューは錨を本文の行で動かし、追従をやめる（ADR 0155 の決定 7）。
    if (const PreviewView *preview = state_.preview(); preview != nullptr)
    {
        PreviewView view = *preview;
        const core::TextBuffer &text = state_.text();
        const core::LineNumber top = settled_scroll().first_visible;
        const core::Offset base = view.follows ? text.line_start(top) : view.anchor;
        const auto line = static_cast<std::int64_t>(text.position_of(base).line.value);
        const auto last = static_cast<std::int64_t>(text.line_count());
        const auto moved = std::clamp(line + std::int64_t{intent.lines}, std::int64_t{1}, last);
        view.anchor = text.line_start(core::LineNumber{static_cast<std::size_t>(moved)});
        view.follows = false;
        state_ = state_.with_preview(std::move(view));
        preview_body_top_ = top;
        return;
    }
    const auto pane = state_.other_pane_state();
    if (!pane.has_value())
    {
        return;
    }
    const EditorState &state = pane.value();
    VisualTextView view(state, ports_.text_layouts,
                        display_pending(state.display_inquiry()) ? core::TextViewGeometry{}
                                                                 : view_geometry_);
    const ScrollState &scroll = state.scroll();
    const auto moved =
        view.shifted(core::VisualPosition{scroll.first_visible, scroll.first_row}, intent.lines)
            .and_then([&view, this](core::VisualPosition at)
                      { return view.constrained(at, scroll_extent_for(state_.mode())); });
    if (!moved)
    {
        layout_failure_ = moved.error();
        return;
    }
    state_ = state_.with_other_scrolled(moved.value().line, moved.value().row);
}

core::TitleBarInput EditorController::title_bar_input() const
{
    return title_bar_input(state_.title_bar_width(), core::reference_dpi);
}

void EditorController::reveal_active_tab()
{
    const std::int32_t scroll =
        state_.title_bar_width() > 0 ? core::tabs_scrolled_into_view(title_bar_input()) : 0;
    state_ = state_.with_tab_scroll(scroll);
}

void EditorController::accept(const TitleBarWidth &intent)
{
    state_ = state_.with_title_bar_width(std::max(intent.dip, 0));
    reveal_active_tab();
}

void EditorController::accept(const ScrollTabs &intent)
{
    if (state_.title_bar_width() <= 0)
    {
        return;
    }
    state_ = state_.with_tab_scroll(core::tabs_scrolled_by(title_bar_input(), intent.notches));
}

void EditorController::accept(const PointTitleBar &intent)
{
    state_ = state_.with_hovered(intent.target);
}
} // namespace nenenib::application
