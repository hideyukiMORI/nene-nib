#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "DevicePixels.hpp"
#include "DisplayInquiry.hpp"
#include "DisplayText.hpp"
#include "Edit.hpp"
#include "ExWindowCommand.hpp"
#include "PaneLayouts.hpp"
#include "PaneSizing.hpp"
#include "PaneView.hpp"
#include "PinTab.hpp"
#include "PreviewView.hpp"
#include "SplitLayout.hpp"
#include "TabReach.hpp"
#include "TabRecency.hpp"
#include "TabTip.hpp"
#include "TabTipPhase.hpp"
#include "TabTipTimer.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "UiMessage.hpp"
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
using detail::not_user_operation;
using detail::parked_path;
using detail::scroll_extent_for;
using detail::tab_path_at;

namespace detail
{
// 帯の位置 position のタブのパス（無題なら無し）。脇の束は帯の位置からアクティブを抜いた順。
std::optional<core::FilePath> tab_path_at(const EditorState &state, std::size_t position)
{
    if (position == state.active_tab())
    {
        return state.document().path;
    }
    const std::size_t parked = position < state.active_tab() ? position : position - 1;
    return std::visit([](const auto &value) { return std::optional{parked_path(value)}; },
                      state.parked().at(parked));
}
} // namespace detail

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
    // 指されたタブが待っている切り替え先を置き換える（ADR 0159 の追補 6）。
    reach_wanted_.reset();
    // アクティブ自身は文書を動かさず、歩きの途中なら着いた所で確定する（ADR 0058 の決定 2）。
    if (intent.index == state_.active_tab())
    {
        state_ = state_.with_switched(intent.index);
        return;
    }
    // 行き先がまだ読んでいない文書ならここで読む。読めなければそのタブは外れて知らせだけが残り、
    // 今の文書の一時の値（入力行・変換中の文字列）は閉じない（ADR 0059 の決定 5）。裏で読んで
    // いれば、揃ったときに切り替える先として覚える（追補 1・6）。
    switch (reach_tab(intent.index))
    {
    case TabReach::reached:
        break;
    case TabReach::reading:
        reach_wanted_ = state_.autosave_ids().at(intent.index);
        return;
    case TabReach::dropped:
        return;
    }
    leave_document();
    state_ = state_.with_switched(intent.index);
    enter_document();
}

void EditorController::accept(const WalkRecentTab &intent)
{
    auto destination = core::tab_recency_walked(state_.recency(), state_.active_tab(), intent.step);
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
    // 行き先が裏の読みの途中なら、帯に残したまま飛ばして次の行き先へ進む（ADR 0159 の追補 6）。
    // 一周して今のタブへ戻れば候補が全部読みの途中で、動かない。行き先が読めなければ外れて、
    // 歩きは今のタブのまま続く。次の 1 歩は外れた後の使った順の隣へ行く（ADR 0059 の決定 5）。
    while (destination.has_value() && destination.value() != state_.active_tab())
    {
        switch (reach_tab(destination.value()))
        {
        case TabReach::reached:
            leave_document();
            state_ = state_.with_walked(destination.value());
            enter_document();
            return;
        case TabReach::reading:
            destination =
                core::tab_recency_walked(state_.recency(), destination.value(), intent.step);
            break;
        case TabReach::dropped:
            return;
        }
    }
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

// 題名のダブルクリックと明示の開く（ADR 0179 の決定 4-3〜4-5）。指された位置のタブへ切り替え、
// 切り替えが成って（アクティブがそのタブの退避番号になって）そのタブが印のタブなら固定する。
// 位置ではなく番号で比べるのは、切り替えが読めないタブを帯から外すと位置がずれるから（#1427 の
// L1）。印のタブでなければ切り替えるだけ。
void EditorController::accept(const PinTab &intent)
{
    if (intent.index >= state_.tab_count())
    {
        return;
    }
    const AutosaveId pointed = state_.autosave_ids().at(intent.index);
    accept(SwitchTab{intent.index});
    if (state_.autosave_ids().at(state_.active_tab()) == pointed)
    {
        pin_active_tab();
    }
}

void EditorController::pin_active_tab()
{
    if (state_.transient_tab() == state_.active_tab())
    {
        state_ = state_.without_transient();
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
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_pane_already_split));
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
            ui_notice(core::UiMessage::notice_pane_preview_unreachable));
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
        reach_wanted_ = other->tab;
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
    // 全面プレビューの間の「枠を閉じる」と `:only` は全面を抜けるだけ（隠した本文の枠は残す・
    // ADR 0155 の追補 33）。
    if (state_.full_preview() != nullptr)
    {
        leave_full_preview(PreviewPlacement::off);
        return;
    }
    state_ = state_.without_split();
}

// もう 1 つの枠だけを送る（決定 11）。送り方と収め方は ScrollLines と同じ 2 本（shifted と
// constrained）を、もう 1 つの枠を操作中の枠の形で読む写しに通す。
void EditorController::accept(const ScrollPane &intent)
{
    // プレビューは送りの量を積み、追従をやめる（ADR 0155 の決定 7・畳むのは ui・#1894）。
    if (state_.preview() != nullptr)
    {
        scroll_preview(intent.lines);
        return;
    }
    const auto pane = other_pane_viewed();
    if (!pane.has_value())
    {
        return;
    }
    const EditorState &state = pane.value();
    VisualTextView view(state, ports_.text_layouts,
                        display_pending(state.display_inquiry()) ? core::TextViewGeometry{}
                                                                 : other_geometry());
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

// 境界が動いた後のもう 1 つの枠（追補の決定 5）。操作中の枠の clamp_scroll と同じく先頭の論理行と
// 画面行を保ち、その枠の寸法と行数で constrained に収める。プレビューの枠・寸法が届く前・問いの
// 答えを待つ文書は触らない。
void EditorController::clamp_other_scroll()
{
    const auto pane = other_pane_viewed();
    if (!pane.has_value() || other_geometry().width <= 0 ||
        display_pending(pane.value().display_inquiry()))
    {
        return;
    }
    const EditorState &state = pane.value();
    VisualTextView view(state, ports_.text_layouts, other_geometry());
    const ScrollState &scroll = state.scroll();
    const auto within =
        view.constrained(core::VisualPosition{scroll.first_visible, scroll.first_row},
                         scroll_extent_for(state_.mode()));
    if (!within)
    {
        layout_failure_ = within.error();
        return;
    }
    if (within.value().line != scroll.first_visible || within.value().row != scroll.first_row)
    {
        state_ = state_.with_other_scrolled(within.value().line, within.value().row);
    }
}

core::TitleBarInput EditorController::title_bar_input() const
{
    return title_bar_input(state_.title_bar_width(), core::reference_dpi);
}

core::PaneSplit EditorController::pane_split() const noexcept
{
    return core::PaneSplit{state_.split().layout, state_.split().share};
}

core::FontSize EditorController::font_size() const noexcept
{
    return state_.settings().font_size;
}

const core::TextViewGeometry &EditorController::other_geometry() const noexcept
{
    return other_lines_ > 0 ? other_geometry_ : view_geometry_;
}

// もう 1 つの枠の写しの見えている行数は、その枠の行数（ADR 0148 の 2026-11-22 の追補の決定 5）。
std::optional<EditorState> EditorController::other_pane_viewed() const
{
    auto pane = state_.other_pane_state();
    if (!pane.has_value() || other_lines_ == 0)
    {
        return pane;
    }
    auto scroll = pane.value().scroll();
    scroll.visible_lines = other_lines_;
    return pane.value().with_scroll(scroll);
}

// 境界のドラッグ（追補の決定 3）。比は窓が範囲に収めた値で、状態に置くだけ。
void EditorController::accept(const ResizeSplit &intent)
{
    state_ = state_.with_split_share(intent.share);
}

void EditorController::accept(const ResizePane &intent)
{
    resize_pane(intent.sizing);
}

// 鍵の 1 回（追補の決定 4）。寸法が届く前と 1 枠は何もしない。first は今の比から作り直す（同じ
// 鍵列の中で続けて変えても、窓が寸法を送り直す前の古い値を読まない）。升は左右なら本文の書式の
// `0` の幅、上下なら行の高さ。
void EditorController::resize_pane(core::PaneSizing sizing)
{
    if (!split_extent_.has_value() || state_.split().layout == core::SplitLayout::none)
    {
        return;
    }
    auto extent = split_extent_.value();
    extent.first = core::split_first_length(extent, state_.split().share);
    const auto cell = extent.dimension == core::PaneDimension::width
                          ? static_cast<std::int32_t>(view_geometry_.average_advance + 0.5F)
                          : view_geometry_.line_height;
    const auto share = core::sized_share(extent, state_.split().active_side, sizing, cell);
    if (share.has_value())
    {
        state_ = state_.with_split_share(share.value());
    }
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

// 載せたタブ（本体か ×）が変わったときだけ札の待ちを始め直す。タブを離れたら待ちと札を消す。同じ
// タブの本体と × の間の動きは待ちも札も保つ（#1789）。
void EditorController::accept(const PointTitleBar &intent)
{
    const auto before = core::tab_tip_tab(state_.hovered());
    const auto after = core::tab_tip_tab(intent.target);
    state_ = state_.with_hovered(intent.target);
    if (intent.pressed)
    {
        hide_tab_tip();
        return;
    }
    if (before == after)
    {
        return;
    }
    if (after.has_value())
    {
        state_ = state_.with_tab_tip(TabTipPhase::waiting);
        tab_tip_timer_ = TabTipTimer::start;
        return;
    }
    hide_tab_tip();
}

// 待ちか札があれば消してタイマーを外す。出していなければ何もしない。
void EditorController::hide_tab_tip()
{
    if (state_.tab_tip() == TabTipPhase::hidden)
    {
        return;
    }
    state_ = state_.with_tab_tip(TabTipPhase::hidden);
    tab_tip_timer_ = TabTipTimer::stop;
}

// 待っていたタブにまだ載っていて、窓がアクティブで、面も入力行も右クリックのメニュー（#1051）も
// 開いていなければ札を出す。何も
// 変えなければ描き直さない（合図の形・ADR 0155 の追補・#813）。非アクティブの窓の合図は待ちを
// 終える（載せ直すまで出さない）。
void EditorController::accept(const TabTipTick &intent)
{
    repaint_ = false;
    if (state_.tab_tip() != TabTipPhase::waiting)
    {
        return;
    }
    if (!core::tab_tip_tab(state_.hovered()).has_value() || state_.command_input().has_value() ||
        context_menu_active() || !intent.window_active)
    {
        state_ = state_.with_tab_tip(TabTipPhase::hidden);
        return;
    }
    repaint_ = true;
    state_ = state_.with_tab_tip(TabTipPhase::shown);
}

// キャレットの動きの進みを覚える（ADR 0192 の決定 3）。描き直しは要る（renderer が四隅を進める）。
void EditorController::accept(const CaretMotionTick &intent)
{
    caret_motion_clock_ = intent.progress;
}

// 札を消す意図を決める 1 か所（#1789）。使う人の操作（押す・鍵・ホイール・面や入力行を開く…・
// not_user_operation でない意図）は写す前に待ちと札を消す。合図・窓の寸法・外観・帯と面の hover
// は消さない（帯の hover は accept が載せたタブで決める）。
void EditorController::settle_tab_tip(const EditorIntent &intent)
{
    tab_tip_timer_.reset();
    if (!not_user_operation(intent))
    {
        hide_tab_tip();
    }
}

std::optional<std::size_t> EditorController::shown_tab_tip() const
{
    const auto tab = core::tab_tip_tab(state_.hovered());
    if (state_.tab_tip() != TabTipPhase::shown || !tab.has_value() ||
        tab.value() >= state_.tab_count())
    {
        return std::nullopt;
    }
    return tab;
}
} // namespace nenenib::application
