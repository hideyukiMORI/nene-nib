#include "VimStepDetail.hpp"

#include "VimAction.hpp"
#include "VimActionGroup.hpp"
#include "VimCaret.hpp"
#include "VimKeySource.hpp"
#include "VimKeyTable.hpp"
#include "VimMarkEdits.hpp"
#include "VimMotionRange.hpp"
#include "VimOperatorColumns.hpp"
#include "VimVisualRange.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace nenenib::core
{
using detail::aligned_screen;
using detail::backward_characters;
using detail::block_changed;
using detail::block_inserted;
using detail::block_line_end_state;
using detail::block_of;
using detail::block_operated;
using detail::block_removed_to_line_end;
using detail::cancelled;
using detail::case_changed;
using detail::cased_block;
using detail::changed;
using detail::count_digit_dropped;
using detail::count_of;
using detail::counted;
using detail::counts_as_digit;
using detail::display_view;
using detail::drops_count_digit;
using detail::entered_insert;
using detail::failed;
using detail::forward_characters;
using detail::horizontal_action;
using detail::input_action;
using detail::JoinedLines;
using detail::jumped_by;
using detail::laid_out_motion;
using detail::landed_column;
using detail::landing_of;
using detail::line_of;
using detail::lines_joined;
using detail::moved_or_failed;
using detail::range_shift_fits;
using detail::removed_exactly;
using detail::requested_layout;
using detail::requested_viewport;
using detail::reselected_visual;
using detail::restored_at;
using detail::scroll_action;
using detail::search_action;
using detail::ShiftBy;
using detail::shifted;
using detail::shifted_block;
using detail::single_step;
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
    next.wanted_column = wanted_after(
        wanted, landed_column(view.text, state.mode, view.selection.anchor, moved), motion.value());
    return jumped_by(moved_or_failed(VimStep{std::move(next),
                                             VimSelect{Selection{view.selection.anchor, moved}}},
                                     motion.value(), view.selection.caret, landing),
                     view, motion.value());
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
    range.restore = visual_restore(state, view.text, view.selection);
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
        // u の戻り先は range.restore（Vim の oap->start・#1109）、キャレットはそれを置き換えた後の
        // 行と桁へ写した位置（case_changed の cased_caret・#1170）。
        return case_changed(state, view.text, range, operation);
    case VimOperator::shift_right:
    case VimOperator::shift_left:
        // VISUAL の回数は字下げを動かす回数（Vim の op_shift の amount）。文書の上限を超えるなら
        // 作る前に断る（#1341）。
        if (!range_shift_fits(view.text, range, ShiftBy{operation, count_of(state.count)},
                              view.document_limit))
        {
            return failed(cancelled(state), VimRepeatFailure::too_large);
        }
        return shifted(state, view.text, range, ShiftBy{operation, count_of(state.count)});
    }
    std::unreachable();
}

// VISUAL の `~` `u` `U` `>` `<` と `g~` `gu` `gU`（決定 10・11）。矩形の大小文字は行ごとに効き
// （ADR 0035 の追補 #959）、矩形の `>` `<` は矩形の左の桁から字下げを動かす（追補 #1042）。
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
    case VimOperator::shift_right:
    case VimOperator::shift_left:
        return shifted_block(state, view, ShiftBy{operation, count_of(state.count)});
    case VimOperator::remove:
    case VimOperator::change:
    case VimOperator::yank:
        return failed(visual_unchanged(state), VimRepeatFailure::refused);
    }
    std::unreachable();
}

// VISUAL の `c`。矩形は消してから左の桁で INSERT に入る（ADR 0035 の追補 #1042）。矩形の形は
// 選んだまま（`$` で選んだなら行末まで）なので、`$` で取り直さない at_column を渡す。
[[nodiscard]] VimStep visual_changed(const VimState &state, const VimEditorView &view)
{
    return state.mode == VimMode::visual_block
               ? block_changed(state, view, VimColumnWish::at_column)
               : visual_operated(state, view, VimOperator::change);
}

// VISUAL の `J` `gJ`（#1417・Vim の nv_join が VISUAL では nv_operator へ流す OP_JOIN）。選んだ行を
// つなぎ、1 行だけなら次の行までの 2 行（回数は使わない・`V3J` も 2 行）。種類を問わない。2 行目が
// 本文の外なら鳴らして VISUAL を抜け、選択の始めに残る（`GVJ`）。つなぐ本体は NORMAL の `J` と
// 同じ lines_joined で、u の戻り先は VISUAL の `d` と同じ visual_restore（`lVjJu` は行頭・
// `jlVkJu` はキャレット・実測）。
[[nodiscard]] VimStep visual_joined(const VimState &state, const VimEditorView &view,
                                    bool inserts_space)
{
    const TextBuffer &text = view.text;
    const LineNumber anchor = line_of(text, view.selection.anchor);
    const LineNumber caret = line_of(text, view.selection.caret);
    const LineNumber first{std::min(anchor.value, caret.value)};
    // 初期化子並びの std::max は MSVC STL の __std_max_8u を core の外へ出す（ARC-003）ので 2
    // 引数で。
    const LineNumber last{std::max(std::max(anchor.value, caret.value), first.value + 1)};
    const Offset restore = visual_restore(state, text, view.selection);
    if (last.value > text.line_count())
    {
        return failed(VimStep{vim_resting_from(state, state.unnamed_register), VimMoveTo{restore}},
                      VimRepeatFailure::not_moved);
    }
    return restored_at(lines_joined(state, text, JoinedLines{first, last}, inserts_space), restore);
}

// VISUAL の `X` `D` `Y`（#1417・Vim の v_visop）。文字単位と行単位は行単位の `d` `y`（種類だけを
// 行単位に読み替える。前の選択は鍵の前の種類のまま＝visual_remembered が読むのは鍵の前の状態・
// Vim の VIsual_mode_orig）。矩形の `X` `Y` は矩形の `d` `y`、矩形の `D` は行末まで取り直して消す。
// 回数は使わない（`v2X` も選んだ行だけ・実測）。
[[nodiscard]] VimStep visual_linewise_operated(const VimState &state, const VimEditorView &view,
                                               VimAction action)
{
    const VimOperator operation =
        action == VimAction::yank_line ? VimOperator::yank : VimOperator::remove;
    if (state.mode == VimMode::visual_block)
    {
        return action == VimAction::remove_to_line_end ? block_removed_to_line_end(state, view)
                                                       : block_operated(state, view, operation);
    }
    VimState linewise = state;
    linewise.mode = VimMode::visual_line;
    return visual_operated(linewise, view, operation);
}

// VISUAL の鍵が範囲を取るときの種類（Vim の v_visop・nv_subst・nv_Replace・#1417・#1436）。大文字の
// `X` `D` `Y` `C` `I` `A` は矩形でなければ行単位へ、`R` は矩形も行単位へ読み替える。ほかの鍵は
// 種類のまま（`s` は `c`・矩形の大文字は矩形のまま）。前の選択（`gv`）は鍵の前の種類のまま
// （visual_remembered・Vim の VIsual_mode_orig）。
[[nodiscard]] VimMode operated_mode(VimMode mode, VimAction action) noexcept
{
    if (action == VimAction::replace_mode)
    {
        return VimMode::visual_line;
    }
    const bool upper =
        action == VimAction::remove_character_before || action == VimAction::remove_to_line_end ||
        action == VimAction::yank_line || action == VimAction::change_to_line_end ||
        action == VimAction::insert_at_line_start || action == VimAction::insert_at_line_end;
    return upper && mode != VimMode::visual_block ? VimMode::visual_line : mode;
}

// 欲しい列が行末（`$`・Vim の curswant == MAXCOL）か。
[[nodiscard]] bool at_line_end(const VimState &state) noexcept
{
    return state.wanted_column.has_value() &&
           state.wanted_column.value().wish == VimColumnWish::at_line_end;
}

// `.` が VISUAL の変更を再生している鍵か（Vim の redo_VIsual_busy・visual_remembered と同じ問い）。
[[nodiscard]] bool redoing(const VimState &state, const VimEditorView &view) noexcept
{
    return state.redoing_visual && vim_key_replayed(view.source);
}

// オペレータが取る範囲の両端（Vim の do_pending_operator）。行単位なら `oap->start = VIsual` の桁を
// 0 にしてからカーソルと並べる（選択の始めの端だけを行頭へ寄せ、カーソルの桁は残す・`jlvkIX` は
// 1 行目の 2 桁目に打つ）。`.` の再生の中は `oap->start` がカーソル（選び直した始めの端）で、
// 終わりは同じ桁（バイト）のまま行数ぶん下（coladvance しない・`lVjIX<Esc>jl.` は 2 行目の 2 桁目・
// `vlAX<Esc>j0.` は 2 行目の 1 文字目の後ろ・実測）。
[[nodiscard]] OffsetRange operated_ends(const VimState &state, const VimEditorView &view,
                                        VimMode mode)
{
    const TextBuffer &text = view.text;
    Offset anchor = view.selection.anchor;
    if (mode == VimMode::visual_line && redoing(state, view))
    {
        // 記録が `$` で選んだ大きさなら Vim は coladvance(MAXCOL) で行の最後の文字へ置く
        // （rv_vcol == MAXCOL・`v$AX<Esc>j0.`・#1436）。
        const LineNumber last = line_of(text, view.selection.caret);
        const std::size_t byte = at_line_end(state)
                                     ? text.line_end(last).value - text.line_start(last).value
                                     : anchor.value - text.line_start(line_of(text, anchor)).value;
        return OffsetRange{anchor, vim_line_and_column(text, last, byte)};
    }
    if (mode == VimMode::visual_line)
    {
        anchor = text.line_start(line_of(text, anchor));
    }
    const Offset caret = view.selection.caret;
    return caret < anchor ? OffsetRange{caret, anchor} : OffsetRange{anchor, caret};
}

// VISUAL のオペレータが覚え直す仮想桁（#1436・Vim の do_pending_operator）。矩形は左右の桁（再生の
// 中も get_op_vcol が取り直す）。ほかは `$` でなく再生の中でもなければ、終わりの文字の最後の桁を、
// 1 行だけなら始まりの最初の桁も書き直し、残りは前のオペレータの値のまま（resel_VIsual_vcol を
// 測る枝）。
[[nodiscard]] VimOperatorColumns operator_columns_of(const VimState &state,
                                                     const VimEditorView &view, VimMode mode)
{
    if (mode == VimMode::visual_block)
    {
        const VimBlockRange block = block_of(state, view);
        return VimOperatorColumns{
            block.left, VirtualColumn{block.left.value + block.width.columns - single_step}};
    }
    VimOperatorColumns columns = state.operator_columns;
    if (redoing(state, view) || at_line_end(state))
    {
        return columns;
    }
    const OffsetRange ends = operated_ends(state, view, mode);
    columns.end = virtual_column_end(view.text, ends.end);
    if (line_of(view.text, ends.begin) == line_of(view.text, ends.end))
    {
        columns.start = virtual_column(view.text, ends.begin);
    }
    return columns;
}

// 文字単位と行単位の VISUAL の `C` と、全部の種類の `R`（#1436・Vim の v_visop と nv_Replace）。
// 種類だけを行単位に読み替えて VISUAL の `c` へ流す（範囲・レジスタ・`"1` の規則・u の戻り先は
// `Vc` と同じ道）。回数は使わない（`v2C` `v3R` も選んだ行だけ・実測）。
[[nodiscard]] VimStep visual_linewise_changed(const VimState &state, const VimEditorView &view)
{
    VimState linewise = state;
    linewise.mode = VimMode::visual_line;
    return visual_operated(linewise, view, VimOperator::change);
}

// 文字単位と行単位の VISUAL の `I` `A`（#1436・Vim の nv_edit → v_visop → op_insert）。行単位へ
// 読み替えた範囲の始まりで `I`、終わりで `A` の INSERT に入る（ほかの行へは写さない・回数は Esc で
// 繰り返す＝NORMAL の `I` `A` と同じ入口）。`A` は終わりを行の内容の中へ寄せ（check_cursor_col）、
// 行が空でなく覚え直した 2 つの桁が違うときだけ終わりの文字の後ろに打つ（inc_cursor・`vjAX` は
// 2 行目の頭・`lvjAX` は 2 行目の 2 文字目の後ろ・実測）。u の戻り先は打ち始める所。
[[nodiscard]] VimStep visual_line_inserted(const VimState &state, const VimEditorView &view,
                                           VimAction action)
{
    const OffsetRange ends = operated_ends(state, view, VimMode::visual_line);
    if (action == VimAction::insert_at_line_start)
    {
        return restored_at(entered_insert(state, ends.begin), ends.begin);
    }
    const TextBuffer &text = view.text;
    const LineNumber line = line_of(text, ends.end);
    const bool empty = text.line_start(line) == text.line_end(line);
    Offset end = ends.end;
    if (!empty && end == text.line_end(line))
    {
        end = backward_characters(text, end, single_step);
    }
    const VimOperatorColumns columns = operator_columns_of(state, view, VimMode::visual_line);
    const Offset at =
        empty || columns.start == columns.end ? end : forward_characters(text, end, single_step);
    return restored_at(entered_insert(state, at), at);
}

// 行に効く鍵（`I` `A` `C` `gi` `gI`）。矩形の `I` `A` `C` は ADR 0035 の追補 #1042、文字単位と
// 行単位の `I` `A` `C` は #1436。`gi` は VISUAL では失敗する（実測・#1039）。`J` `gJ` `X` `D` `Y`
// `s` `R` は visual_edit_line が先に振り分ける（`p` `P` は後続・#1418）。
[[nodiscard]] VimStep visual_line_command(const VimState &state, const VimEditorView &view,
                                          VimAction action)
{
    if (action == VimAction::insert_at_last_insert)
    {
        return failed(visual_unchanged(state), VimRepeatFailure::refused);
    }
    // `gI` は Vim の nv_g_cmd と同じく beginline(0) で選択の端を 1 桁目へ動かしてから鳴る
    // （`0` と同じ移動・#1032 の実測）。
    if (action == VimAction::insert_at_column_one)
    {
        return failed(visual_moved(state, view, VimAction::move_line_start),
                      VimRepeatFailure::refused);
    }
    const bool inserts =
        action == VimAction::insert_at_line_start || action == VimAction::insert_at_line_end;
    if (state.mode != VimMode::visual_block)
    {
        if (inserts)
        {
            return visual_line_inserted(state, view, action);
        }
        return action == VimAction::change_to_line_end ? visual_linewise_changed(state, view)
                                                       : visual_unchanged(state);
    }
    if (inserts)
    {
        return block_inserted(state, view, action);
    }
    if (action == VimAction::change_to_line_end)
    {
        return block_changed(state, view, VimColumnWish::at_line_end);
    }
    return visual_unchanged(state);
}

// 行に効く本文の変更の群（NORMAL の normal_line_action と同じ群）の VISUAL の写し先（#1417）。
[[nodiscard]] VimStep visual_edit_line(const VimState &state, const VimEditorView &view,
                                       VimAction action)
{
    if (action == VimAction::join_lines || action == VimAction::join_lines_raw)
    {
        return visual_joined(state, view, action == VimAction::join_lines);
    }
    if (action == VimAction::remove_character_before || action == VimAction::remove_to_line_end ||
        action == VimAction::yank_line)
    {
        return visual_linewise_operated(state, view, action);
    }
    // `s` は種類のまま `c`、`R` は行単位の `c`（Vim の nv_subst と nv_Replace・#1436）。
    if (action == VimAction::substitute)
    {
        return visual_changed(state, view);
    }
    if (action == VimAction::replace_mode)
    {
        return visual_linewise_changed(state, view);
    }
    return visual_line_command(state, view, action);
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

// VISUAL を抜ける鍵がオペレータか（Vim では do_pending_operator を通る鍵・#1436）。範囲の変更・
// `u` `U`・`Ctrl-a` `Ctrl-x`・行に効く命令がそう。VISUAL の `p` `P` も nv_put_opt が選択を消す
// ために do_pending_operator を通すので含む（#1418 で VISUAL を抜けるようになったら書く）。
// 移動・選択・検索・タブは違う。
[[nodiscard]] bool operates(VimActionGroup group) noexcept
{
    switch (group)
    {
    case VimActionGroup::edit_range:
    case VimActionGroup::history:
    case VimActionGroup::number:
    case VimActionGroup::insert_line:
    case VimActionGroup::edit_line:
        return true;
    case VimActionGroup::motion:
    case VimActionGroup::scroll:
    case VimActionGroup::enter_visual:
    case VimActionGroup::insert_object:
    case VimActionGroup::input_wait:
    case VimActionGroup::search:
    case VimActionGroup::tab:
    case VimActionGroup::horizontal_scroll:
    case VimActionGroup::screen_alignment:
    case VimActionGroup::jump_list:
    case VimActionGroup::ex_line:
        return false;
    }
    std::unreachable();
}

// 分類ごとの写し先（CPP-012 / ADR 0006）。分類が増えたらここで落ちる。
[[nodiscard]] VimStep visual_grouped(const VimState &state, const VimEditorView &view,
                                     VimAction action, VimActionGroup group)
{
    switch (group)
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
    // `Ctrl-o` `Ctrl-i` `g;` `g,` は VISUAL では鳴らすだけ（Vim の checkclearopq・#1037）。
    case VimActionGroup::jump_list:
        return failed(visual_unchanged(state), VimRepeatFailure::refused);
    case VimActionGroup::history:
        return visual_history(state, view, action);
    case VimActionGroup::number:
        return visual_numbered(state, view, action);
    case VimActionGroup::insert_line:
        return visual_line_command(state, view, action);
    case VimActionGroup::edit_line:
        return visual_edit_line(state, view, action);
    case VimActionGroup::ex_line:
        return visual_unchanged(state);
    }
    std::unreachable();
}
} // namespace

namespace detail
{
[[nodiscard]] VimMode visual_operated_mode(VimMode mode, VimAction action) noexcept
{
    return operated_mode(mode, action);
}

// VISUAL をオペレータで抜けた歩に、覚え直した仮想桁を置く 1 か所（#1436・Vim の
// do_pending_operator）。`mode` は鍵が範囲を取る種類（operated_mode）。VISUAL に残る歩と、VISUAL
// でない鍵の前の状態は変えない。`r{char}` の道（VimStepReplace）もここを通す。
[[nodiscard]] VimStep visual_operation_noted(const VimState &state, const VimEditorView &view,
                                             VimAction action, VimStep step)
{
    if (!is_visual_mode(state.mode) || is_visual_mode(step.next.mode))
    {
        return step;
    }
    step.next.operator_columns = operator_columns_of(block_line_end_state(state, action), view,
                                                     operated_mode(state.mode, action));
    return step;
}

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
    VimStep step = visual_grouped(state, view, action, group.value());
    if (!operates(group.value()))
    {
        return step;
    }
    return visual_operation_noted(state, view, action, std::move(step));
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
    // VISUAL の Ctrl-o は Vim の nv_pcmark が checkclearopq で失敗する（#1037）。
    case VimSpecialKey::control_o:
        return failed(visual_unchanged(state), VimRepeatFailure::refused);
    case VimSpecialKey::control_e:
        return visual_acted(state, view, VimAction::scroll_line_down);
    case VimSpecialKey::control_y:
        return visual_acted(state, view, VimAction::scroll_line_up);
    // VISUAL の `<Del>` は `x`（Vim の nv_abbrev → v_visop・#1325）。回数の途中は桁を消す。
    case VimSpecialKey::delete_key:
        if (drops_count_digit(state, VimKey{key}))
        {
            return count_digit_dropped(state);
        }
        return visual_character(state, view, VimCharacter{U'x'});
    }
    std::unreachable();
}
} // namespace detail
} // namespace nenenib::core
