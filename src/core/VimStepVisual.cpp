#include "VimStepDetail.hpp"

#include "VimAction.hpp"
#include "VimActionGroup.hpp"
#include "VimCaret.hpp"
#include "VimKeyTable.hpp"
#include "VimMarkEdits.hpp"
#include "VimMotionRange.hpp"
#include "VimVisualRange.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace nenenib::core
{
using detail::aligned_screen;
using detail::block_operated;
using detail::case_changed;
using detail::cased_block;
using detail::changed;
using detail::count_of;
using detail::counted;
using detail::counts_as_digit;
using detail::display_view;
using detail::failed;
using detail::horizontal_action;
using detail::input_action;
using detail::jumped_by;
using detail::laid_out_motion;
using detail::landing_of;
using detail::line_of;
using detail::moved_or_failed;
using detail::removed_exactly;
using detail::requested_layout;
using detail::requested_viewport;
using detail::reselected_visual;
using detail::restored_at;
using detail::scroll_action;
using detail::search_action;
using detail::ShiftBy;
using detail::shifted;
using detail::tab_action;
using detail::visual_acted;
using detail::visual_character;
using detail::visual_left;
using detail::visual_numbered;
using detail::visual_resting;
using detail::visual_restore;
using detail::visual_special;
using detail::wanted_after;
using detail::wanted_for;
using detail::window_started;
using detail::yanked;

namespace detail
{
// ---------------------------------------------------------------- VISUAL（ADR 0018）

// 鍵を 1 つ食べ終わったあとの VISUAL。回数とオペレータは捨て、モードと無名レジスタだけ残る。
[[nodiscard]] VimState visual_resting(const VimState &state, VimMode mode)
{
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.mode = mode;
    return next;
}

// 抜ける VISUAL を前の選択として覚える値（ADR 0018 の追補・#1039）。種類・両端・欲しい列は
// 鍵の前の値で、Vim の end_visual_mode と do_pending_operator が b_visual に置くのと同じ。
[[nodiscard]] VimLastVisual visual_left(const VimState &state, const VimEditorView &view)
{
    return VimLastVisual{state.mode, vim_mark_position_of(view.text, view.selection.anchor),
                         vim_mark_position_of(view.text, view.selection.caret),
                         state.wanted_column};
}

// `gv`（#1039）。前の選択を種類・両端・欲しい列ごと選び直す。前の選択が無いか、始めの端の行が
// 本文の外なら空（Vim の nv_gv のビープ・最終行の `dd` の後で実測）。キャレットの端の行は最後の
// 行へ寄せ、桁はどちらも行の内容の終わりまで（Vim の check_cursor）。
[[nodiscard]] std::optional<VimStep> reselected_visual(const VimState &state,
                                                       const VimEditorView &view)
{
    const std::optional<VimLastVisual> &last = view.marks->last_visual;
    if (!last.has_value())
    {
        return std::nullopt;
    }
    const VimLastVisual &previous = last.value();
    const auto anchor = vim_mark_offset(view.text, previous.anchor);
    if (!anchor.has_value())
    {
        return std::nullopt;
    }
    const LineNumber line{std::min(previous.caret.line.value, view.text.line_count())};
    const Offset caret = vim_line_and_column(view.text, line, previous.caret.byte);
    VimState next = visual_resting(state, previous.mode);
    next.wanted_column = previous.wanted;
    return VimStep{std::move(next), VimSelect{Selection{anchor.value(), caret}}};
}
} // namespace detail

namespace
{
// VISUAL で効かない鍵。選択もキャレットも本文も動かない（決定 7）。
[[nodiscard]] VimStep visual_unchanged(const VimState &state)
{
    return VimStep{visual_resting(state, state.mode), VimNoEffect{}};
}

// VISUAL から出る（Esc・同じ鍵）。選択は畳み、キャレットはその場に残す（決定 3）。
[[nodiscard]] VimStep left_visual(const VimState &state, const Selection &selection)
{
    return VimStep{vim_resting_from(state, state.unnamed_register), VimMoveTo{selection.caret}};
}

// 同じ鍵なら出る、違う鍵なら選択を保ったまま種類を切り替える。
[[nodiscard]] VimStep visual_switched(const VimState &state, const Selection &selection,
                                      VimMode mode)
{
    VimStep step = state.mode == mode ? left_visual(state, selection)
                                      : VimStep{visual_resting(state, mode), VimSelect{selection}};
    step.next.wanted_column = state.wanted_column;
    return step;
}

// 矩形の `O`。行はそのままで、矩形の左右の仮想桁を使い直す（ADR 0035 の決定 3 と追補 #1090・
// Vim の v_swap_corners）。左右は両端の文字の最初の桁の小さいほうと最後の桁の大きいほうで、
// anchor の行を左へ・caret の行を右へ寄せる。caret が動かなければ逆に置く。寄せは欲しい列の
// 着地と同じ offset_at_virtual_column（全角の途中は先頭・短い行は内容の終わり）で、欲しい列は
// caret の行に置いた桁になる（`$` も桁へ戻る・実測）。
[[nodiscard]] VimStep block_column_swapped(const VimState &state, const VimEditorView &view)
{
    const TextBuffer &text = view.text;
    const Selection &selection = view.selection;
    const LineNumber anchor_line = line_of(text, selection.anchor);
    const LineNumber caret_line = line_of(text, selection.caret);
    const VirtualColumn left{std::min(virtual_column(text, selection.anchor).value,
                                      virtual_column(text, selection.caret).value)};
    const VirtualColumn right{std::max(virtual_column_end(text, selection.anchor).value,
                                       virtual_column_end(text, selection.caret).value)};
    const bool caret_stays = offset_at_virtual_column(text, caret_line, right) == selection.caret;
    const VirtualColumn anchor_column = caret_stays ? right : left;
    const VirtualColumn caret_column = caret_stays ? left : right;
    VimState next = visual_resting(state, state.mode);
    next.wanted_column = VimWantedColumn{VimColumnWish::at_column, caret_column};
    return VimStep{std::move(next),
                   VimSelect{Selection{offset_at_virtual_column(text, anchor_line, anchor_column),
                                       offset_at_virtual_column(text, caret_line, caret_column)}}};
}

// VISUAL の移動。anchor はそのままで caret だけ動く（決定 3）。欲しい列は NORMAL と同じ。
[[nodiscard]] VimStep visual_moved(const VimState &state, const VimEditorView &view,
                                   VimAction action)
{
    const auto motion = laid_out_motion(view, action);
    if (!motion.has_value())
    {
        return visual_unchanged(state);
    }
    const VimWantedColumn wanted = wanted_for(view, state, motion.value());
    const VimMotionLanding landing = landing_of(view, state, motion.value());
    const Offset moved = landing.offset;
    VimState next = visual_resting(state, state.mode);
    next.wanted_column = wanted_after(view.text, wanted, moved, motion.value());
    return jumped_by(moved_or_failed(VimStep{std::move(next),
                                             VimSelect{Selection{view.selection.anchor, moved}}},
                                     motion.value(), view.selection.caret, landing),
                     view, motion.value());
}

// VISUAL の大小文字（決定 10）。キャレットは行単位なら最初の行の行頭、文字単位なら選択の先頭
// （Vim の oap->start）。u の戻り先は d と同じ visual_restore。
[[nodiscard]] VimStep visual_case_changed(const VimState &state, const TextBuffer &text,
                                          VimMotionRange range, VimOperator operation)
{
    const Offset restore = range.restore.value_or(range.range.begin);
    range.restore = range.range.begin;
    return restored_at(case_changed(state, text, range, operation), restore);
}

// `d x y c`。選択を範囲に変えて #43 と同じ経路へ流す（決定 5）。オペレータは VISUAL を終わらせる。
[[nodiscard]] VimStep visual_operated(const VimState &state, const VimEditorView &view,
                                      VimOperator operation)
{
    if (state.mode == VimMode::visual_block)
    {
        return block_operated(state, view, operation);
    }
    VimMotionRange range = vim_visual_range(view.text, view.selection, state.mode);
    range.restore = visual_restore(view.text, view.selection, state.mode);
    switch (operation)
    {
    case VimOperator::remove:
        return removed_exactly(state, view.text, range);
    case VimOperator::change:
        return changed(state, view.text, range);
    case VimOperator::yank:
        return yanked(state, view.text, view.selection.caret, range);
    case VimOperator::toggle_case:
    case VimOperator::lower_case:
    case VimOperator::upper_case:
        return visual_case_changed(state, view.text, range, operation);
    case VimOperator::shift_right:
    case VimOperator::shift_left:
        // VISUAL の回数は字下げを動かす回数（Vim の op_shift の amount）。
        return shifted(state, view.text, range, ShiftBy{operation, count_of(state.count)});
    }
    std::unreachable();
}

// VISUAL の `~` `u` `U` `>` `<` と `g~` `gu` `gU`（決定 10・11）。矩形の大小文字は行ごとに効き
// （ADR 0035 の追補 #959）、矩形の `>` `<` は #770 の範囲で、いまは鍵を拒んで矩形のまま残る。
[[nodiscard]] VimStep visual_rewritten(const VimState &state, const VimEditorView &view,
                                       VimOperator operation)
{
    if (state.mode != VimMode::visual_block)
    {
        return visual_operated(state, view, operation);
    }
    switch (operation)
    {
    case VimOperator::toggle_case:
    case VimOperator::lower_case:
    case VimOperator::upper_case:
        return cased_block(state, view, operation);
    case VimOperator::remove:
    case VimOperator::change:
    case VimOperator::yank:
    case VimOperator::shift_right:
    case VimOperator::shift_left:
        return failed(visual_unchanged(state), VimRepeatFailure::refused);
    }
    std::unreachable();
}

// VISUAL の `c`。矩形は範囲外なので鍵を捨てて矩形のまま残る（ADR 0035 の決定 5）。
[[nodiscard]] VimStep visual_changed(const VimState &state, const VimEditorView &view)
{
    return state.mode == VimMode::visual_block ? visual_unchanged(state)
                                               : visual_operated(state, view, VimOperator::change);
}

// VISUAL の `o` `O`。矩形の `O` だけが行を変えずに左右の桁を使い直し、ほかは対角の角へ移る
// （ADR 0018 の決定 3 / ADR 0035 の決定 3 と追補 #1090）。
[[nodiscard]] VimStep visual_swapped(const VimState &state, const VimEditorView &view,
                                     VimAction action)
{
    if (state.mode == VimMode::visual_block && action == VimAction::open_line_above)
    {
        return block_column_swapped(state, view);
    }
    return VimStep{visual_resting(state, state.mode),
                   VimSelect{Selection{view.selection.caret, view.selection.anchor}}};
}

// VISUAL の `gv`（#1039）。今の選択と前の選択を入れ替える（Vim の nv_gv）。前の選択が無ければ
// 失敗して今の選択のまま残る（実測）。
[[nodiscard]] VimStep visual_exchanged(const VimState &state, const VimEditorView &view)
{
    auto step = reselected_visual(state, view);
    if (!step.has_value())
    {
        return failed(visual_unchanged(state), VimRepeatFailure::not_found);
    }
    VimMarks marks = *view.marks;
    marks.last_visual = visual_left(state, view);
    step.value().marks = std::move(marks);
    return std::move(step).value();
}

[[nodiscard]] VimStep visual_selection_action(const VimState &state, const VimEditorView &view,
                                              VimAction action)
{
    if (action == VimAction::remove_character || action == VimAction::remove_operator)
    {
        return visual_operated(state, view, VimOperator::remove);
    }
    if (action == VimAction::change_operator)
    {
        return visual_changed(state, view);
    }
    if (action == VimAction::yank_operator)
    {
        return visual_operated(state, view, VimOperator::yank);
    }
    if (action == VimAction::toggle_case_character)
    {
        return visual_rewritten(state, view, VimOperator::toggle_case);
    }
    if (const auto operation = vim_operator_for(action); operation.has_value())
    {
        return visual_rewritten(state, view, operation.value());
    }
    if (action == VimAction::open_line_below || action == VimAction::open_line_above)
    {
        return visual_swapped(state, view, action);
    }
    if (action == VimAction::visual)
    {
        return visual_switched(state, view.selection, VimMode::visual);
    }
    if (action == VimAction::visual_line)
    {
        return visual_switched(state, view.selection, VimMode::visual_line);
    }
    std::unreachable();
}

// VISUAL の `u` `U` は undo ではなく選択の小文字化・大文字化（ADR 0015 の決定 10・Vim の
// nv_undo / nv_Undo）。ほかの履歴の鍵（Ctrl-r `.`）は VISUAL では効かない。
[[nodiscard]] VimStep visual_history(const VimState &state, const VimEditorView &view,
                                     VimAction action)
{
    if (action == VimAction::undo)
    {
        return visual_rewritten(state, view, VimOperator::lower_case);
    }
    if (action == VimAction::undo_line)
    {
        return visual_rewritten(state, view, VimOperator::upper_case);
    }
    return visual_unchanged(state);
}
} // namespace

namespace detail
{
// VISUAL の写し先（決定 7・決定 8）。行に効く命令・挿入・Ex は VISUAL では効かない。
[[nodiscard]] VimStep visual_acted(const VimState &state, const VimEditorView &input,
                                   VimAction action)
{
    if (auto request = requested_viewport(state, input, action))
    {
        return std::move(request).value();
    }
    const auto displayed = display_view(state, input, action);
    if (!displayed)
    {
        return requested_layout(state, displayed.error());
    }
    const auto &view = displayed.value();
    const auto group = vim_group_for(action);
    if (!group.has_value())
    {
        return visual_unchanged(state);
    }
    switch (group.value())
    {
    case VimActionGroup::motion:
        return visual_moved(state, view, action);
    case VimActionGroup::scroll:
        return scroll_action(state, view, action);
    case VimActionGroup::enter_visual:
        return action == VimAction::reselect_visual ? visual_exchanged(state, view)
                                                    : visual_selection_action(state, view, action);
    case VimActionGroup::edit_range:
        return visual_selection_action(state, view, action);
    case VimActionGroup::insert_object:
        // 矩形の `i` `a`（テキストオブジェクト）は範囲外（ADR 0035 の決定 5）。
        return state.mode == VimMode::visual_block ? visual_unchanged(state)
                                                   : input_action(state, view, action);
    case VimActionGroup::input_wait:
        return input_action(state, view, action);
    case VimActionGroup::search:
        return search_action(state, view, action);
    case VimActionGroup::tab:
        return tab_action(state, view, action, visual_unchanged(state));
    case VimActionGroup::horizontal_scroll:
        return horizontal_action(state, view, action);
    case VimActionGroup::screen_alignment:
        return aligned_screen(state, view, action);
    case VimActionGroup::history:
        return visual_history(state, view, action);
    case VimActionGroup::number:
        return visual_numbered(state, view, action);
    case VimActionGroup::insert_line:
        // `gi` は VISUAL では失敗する（実測・#1039）。
        return action == VimAction::insert_at_last_insert
                   ? failed(visual_unchanged(state), VimRepeatFailure::refused)
                   : visual_unchanged(state);
    case VimActionGroup::edit_line:
    case VimActionGroup::ex_line:
        return visual_unchanged(state);
    }
    std::unreachable();
}

[[nodiscard]] VimStep visual_character(const VimState &state, const VimEditorView &view,
                                       VimCharacter key)
{
    if (counts_as_digit(state, key.code))
    {
        return counted(state, key.code);
    }
    const auto action = vim_action_for(key.code);
    if (!action.has_value())
    {
        return visual_unchanged(state);
    }
    return visual_acted(state, view, action.value());
}

// VISUAL の特別な鍵。Esc で出て、矢印と Home / End は NORMAL と同じ動作を引く。
// Ctrl-r はこの縦切りに無い（決定 7）。Backspace は行をまたぐ h（ADR 0049 の決定 1）。
[[nodiscard]] VimStep visual_special(const VimState &state, const VimEditorView &view,
                                     VimSpecialKey key)
{
    switch (key)
    {
    case VimSpecialKey::escape:
        return left_visual(state, view.selection);
    case VimSpecialKey::enter:
        return visual_acted(state, view, VimAction::move_next_line);
    case VimSpecialKey::backspace:
        return visual_acted(state, view, VimAction::space_left);
    case VimSpecialKey::control_r:
        return visual_unchanged(state);
    case VimSpecialKey::arrow_left:
        return visual_acted(state, view, VimAction::move_left);
    case VimSpecialKey::arrow_right:
        return visual_acted(state, view, VimAction::move_right);
    case VimSpecialKey::arrow_up:
        return visual_acted(state, view, VimAction::move_up);
    case VimSpecialKey::arrow_down:
        return visual_acted(state, view, VimAction::move_down);
    case VimSpecialKey::home:
        return visual_acted(state, view, VimAction::move_line_start);
    case VimSpecialKey::end:
        return visual_acted(state, view, VimAction::move_line_end);
    case VimSpecialKey::page_up:
    case VimSpecialKey::control_b:
        return visual_acted(state, view, VimAction::scroll_page_up);
    case VimSpecialKey::page_down:
    case VimSpecialKey::control_f:
        return visual_acted(state, view, VimAction::scroll_page_down);
    case VimSpecialKey::control_d:
        return visual_acted(state, view, VimAction::scroll_half_down);
    case VimSpecialKey::control_u:
        return visual_acted(state, view, VimAction::scroll_half_up);
    case VimSpecialKey::control_v:
        return visual_switched(state, view.selection, VimMode::visual_block);
    case VimSpecialKey::control_w:
        return window_started(state);
    case VimSpecialKey::control_a:
        return visual_acted(state, view, VimAction::increment_number);
    case VimSpecialKey::control_x:
        return visual_acted(state, view, VimAction::decrement_number);
    }
    std::unreachable();
}
} // namespace detail
} // namespace nenenib::core
