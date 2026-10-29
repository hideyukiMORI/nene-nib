#include "VimStep.hpp"
#include "VimStepDetail.hpp"

#include "ScrollBounds.hpp"
#include "TabDestination.hpp"
#include "TabJump.hpp"
#include "TabJumpDirection.hpp"
#include "VimAction.hpp"
#include "VimActionGroup.hpp"
#include "VimAlignedLine.hpp"
#include "VimCaret.hpp"
#include "VimCharacterSearchInvocation.hpp"
#include "VimClipboardText.hpp"
#include "VimDisplayLineEdge.hpp"
#include "VimHorizontalScroll.hpp"
#include "VimKeyTable.hpp"
#include "VimLayoutNeed.hpp"
#include "VimMarkEdits.hpp"
#include "VimPutSide.hpp"
#include "VimScreenGeometry.hpp"
#include "VimScreenPlace.hpp"
#include "VimScreenRows.hpp"
#include "VimScrollDirection.hpp"
#include "VimTextObjectCancel.hpp"
#include "VimTextObjectOutcome.hpp"
#include "VimTextObjectRange.hpp"
#include "VimTextObjectRequest.hpp"
#include "VisualPosition.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
using detail::aligned_screen;
using detail::appended_recording;
using detail::cancelled;
using detail::caret_on_line;
using detail::carriage_return_character;
using detail::characters_between;
using detail::combined_count;
using detail::completed_character_search;
using detail::completed_text_object;
using detail::count_of;
using detail::counted;
using detail::counts_as_digit;
using detail::decimal_base;
using detail::display_view;
using detail::failed;
using detail::finished_input_wait;
using detail::forward_characters;
using detail::horizontal_action;
using detail::input_action;
using detail::insert_character;
using detail::insert_register_step;
using detail::insert_special;
using detail::insert_started;
using detail::joined;
using detail::jump_list_action;
using detail::laid_out_motion;
using detail::landed_column;
using detail::landed_step;
using detail::last_view_line;
using detail::line_below;
using detail::line_of;
using detail::macro_record_action;
using detail::macro_recorded;
using detail::motion_range;
using detail::motion_step;
using detail::numbered;
using detail::operated;
using detail::operated_on_lines;
using detail::operated_to_line_end;
using detail::operation_of;
using detail::page_step;
using detail::pending_operator;
using detail::performed;
using detail::put_step;
using detail::register_selection_of;
using detail::repeated_change;
using detail::repeated_character_search;
using detail::replacement_key;
using detail::replayed_macro;
using detail::requested_layout;
using detail::requested_viewport;
using detail::reselected_visual;
using detail::resolved_count;
using detail::rested_in;
using detail::rested_mode;
using detail::scroll_action;
using detail::search_action;
using detail::searched_key;
using detail::selected_register;
using detail::shifted_line;
using detail::single_step;
using detail::started_character_search;
using detail::started_macro;
using detail::started_prefix;
using detail::started_replacement;
using detail::swapped_characters;
using detail::tab_action;
using detail::text_object_action;
using detail::vim_recorded;
using detail::visual_acted;
using detail::visual_character;
using detail::visual_left;
using detail::visual_special;
using detail::wanted_after;
using detail::wanted_column_of;
using detail::window_started;
using detail::word_writing_empty;

namespace
{
// Ctrl-r は文字としては制御文字なので、表に載せる値は名前で書く（原文に制御文字を置かない）。
constexpr char32_t control_r_character = 0x12;

constexpr char32_t control_b_character = 0x02;

constexpr char32_t control_d_character = 0x04;

constexpr char32_t control_f_character = 0x06;

constexpr char32_t control_u_character = 0x15;

// ---------------------------------------------------------------- 鍵から動作へ

// i a I A。回数は o / O と同じで、Esc のときに入力を残りの回数だけ繰り返す（ADR 0028 の
// 決定 2 の機構をそのまま使う。`3ifoo<Esc>` が foofoofoo になるのは Vim も同じ）。
[[nodiscard]] VimStep entered_insert(const VimState &state, Offset caret)
{
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.mode = VimMode::insert;
    const std::size_t count = count_of(state.count);
    if (count > single_step)
    {
        next.insert_repeat = VimInsertRepeat{count - single_step, std::vector<VimKey>{}};
    }
    return VimStep{std::move(next), VimMoveTo{caret}};
}

// x。保留中のオペレータの無い削除なので、範囲を作って同じ 1 本（performed）に流す。
[[nodiscard]] VimStep removed_character(const VimState &state, const TextBuffer &text, Offset caret)
{
    return performed(
        state, text, caret,
        characters_between(caret, forward_characters(text, caret, count_of(state.count))));
}

// X。`dh` の範囲（行頭で止まる・行をまたがない）を x と同じ 1 本（performed）に流す。行頭では
// 空の範囲で、`dh` と同じく失敗の印は付けずレジスタも書かない（Vim 9.1 で実測・Issue #206）。
[[nodiscard]] VimStep removed_character_before(const VimState &state, const VimEditorView &view)
{
    const auto range =
        motion_range(view, VimMotion::left, count_of(state.count), VimOperator::remove);
    if (!range.has_value())
    {
        return failed(cancelled(state), VimRepeatFailure::not_moved);
    }
    return performed(state, view.text, view.selection.caret, range.value());
}
} // namespace

namespace detail
{
// 表から引いた移動を文字組みに合わせる。gj gk は画面行の着地が解けていなければ（文字組みが無効・
// 幅 0 や nowrap の text）j k と同じ（ADR 0144 の決定 4・Vim の nv_gomove も nowrap では j k）。
[[nodiscard]] std::optional<VimMotion> laid_out_motion(const VimEditorView &view, VimAction action)
{
    const auto motion = vim_motion_for(action);
    if (view.row_landing.has_value() || !motion.has_value())
    {
        return motion;
    }
    if (motion.value() == VimMotion::display_down)
    {
        return VimMotion::down;
    }
    return motion.value() == VimMotion::display_up ? VimMotion::up : motion;
}
} // namespace detail

namespace
{
// 表から引ける移動は 1 本にまとめる（表に無い動作はここへ来ない）。
[[nodiscard]] VimStep moved_step(const VimState &state, const VimEditorView &view, VimAction action)
{
    const auto motion = laid_out_motion(view, action);
    if (!motion.has_value())
    {
        return cancelled(state);
    }
    return motion_step(state, view, motion.value());
}

[[nodiscard]] std::size_t limited_amount(std::size_t amount, std::size_t total) noexcept
{
    return std::min(amount, total);
}

[[nodiscard]] std::size_t limited_product(std::size_t left, std::size_t right,
                                          std::size_t total) noexcept
{
    if (left == 0 || right == 0)
    {
        return 0;
    }
    return left > total / right ? total : std::min(left * right, total);
}
} // namespace

namespace detail
{
[[nodiscard]] LineNumber shifted_line(LineNumber line, std::size_t amount, std::size_t total,
                                      VimScrollDirection direction) noexcept
{
    switch (direction)
    {
    case VimScrollDirection::up:
        return LineNumber{line.value > amount ? line.value - amount : 1};
    case VimScrollDirection::down:
    {
        const std::size_t current = std::min(line.value, total);
        return LineNumber{current + std::min(amount, total - current)};
    }
    }
    std::unreachable();
}
} // namespace detail

namespace
{
[[nodiscard]] LineNumber half_page_top(const VimEditorView &view, std::size_t amount,
                                       VimScrollDirection direction)
{
    switch (direction)
    {
    case VimScrollDirection::up:
        return shifted_line(vim_viewport_of(view).first_visible, amount, view.text.line_count(),
                            direction);
    case VimScrollDirection::down:
        break;
    }
    const LineNumber last_filled =
        first_visible_within(LineNumber{view.text.line_count()}, view.text.line_count(),
                             vim_viewport_of(view).visible_lines, ScrollExtent::filled_viewport);
    if (vim_viewport_of(view).first_visible.value >= last_filled.value)
    {
        return vim_viewport_of(view).first_visible;
    }
    return shifted_line(vim_viewport_of(view).first_visible, amount, last_filled.value, direction);
}

[[nodiscard]] LineNumber page_top(const VimEditorView &view, std::size_t pages,
                                  VimScrollDirection direction)
{
    if (direction == VimScrollDirection::down &&
        last_view_line(view).value == view.text.line_count())
    {
        return LineNumber{view.text.line_count()};
    }
    const std::size_t height =
        std::max<std::size_t>(vim_viewport_of(view).visible_lines, single_step);
    const std::size_t one_page = height <= 2 ? height : height - 2;
    const std::size_t amount = limited_product(one_page, pages, view.text.line_count());
    return shifted_line(vim_viewport_of(view).first_visible, amount, view.text.line_count(),
                        direction);
}

// 回数つきの Ctrl-f / Ctrl-b が途中の周で端に当たったか（Vim の onepage の FAIL）。端に当たった周は
// 画面の先頭の行を動かさないので、回数を 1 つ減らしても同じ先頭の行になる。
[[nodiscard]] bool page_count_reached_the_end(const VimEditorView &view, std::size_t pages,
                                              VimScrollDirection direction)
{
    return pages > single_step &&
           page_top(view, pages, direction) == page_top(view, pages - single_step, direction);
}

[[nodiscard]] LineNumber previous_page_top(const VimEditorView &view, std::size_t pages)
{
    const std::size_t height =
        std::max<std::size_t>(vim_viewport_of(view).visible_lines, single_step);
    const std::size_t one_page = height <= 2 ? height : height - 2;
    const std::size_t distance = vim_viewport_of(view).first_visible.value - single_step;
    const std::size_t steps_to_top = distance / one_page + (distance % one_page != 0 ? 1 : 0);
    const std::size_t effective = std::min(pages, steps_to_top);
    const std::size_t previous_steps = effective > 0 ? effective - single_step : 0;
    const std::size_t amount = limited_product(one_page, previous_steps, view.text.line_count());
    return shifted_line(vim_viewport_of(view).first_visible, amount, view.text.line_count(),
                        VimScrollDirection::up);
}

[[nodiscard]] Selection navigated_selection(const VimState &state, const Selection &selection,
                                            Offset caret) noexcept
{
    switch (state.mode)
    {
    case VimMode::normal:
    case VimMode::insert:
        return collapsed_at(caret);
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return Selection{selection.anchor, caret};
    }
    std::unreachable();
}

[[nodiscard]] VimState navigated_state(const VimState &state, const VimEditorView &view,
                                       Offset caret)
{
    VimState next = state;
    next.count = std::nullopt;
    next.pending = std::nullopt;
    next.wanted_column =
        VimWantedColumn{VimColumnWish::at_column,
                        landed_column(view.text, state.mode, view.selection.anchor, caret)};
    return next;
}

[[nodiscard]] VimStep navigated(const VimState &state, const VimEditorView &view, LineNumber first,
                                LineNumber caret_line)
{
    const Offset caret = vim_first_non_blank(view.text, view.text.line_start(caret_line));
    return VimStep{navigated_state(state, view, caret),
                   VimNavigate{navigated_selection(state, view.selection, caret), first}};
}

[[nodiscard]] VimStep navigated_at(const VimState &state, const VimEditorView &view,
                                   LineNumber first, Offset caret)
{
    return VimStep{navigated_state(state, view, caret),
                   VimNavigate{navigated_selection(state, view.selection, caret), first}};
}

// 画面行の単位の着地（ADR 0144 の決定 5・6）。置き所をモードで寄せ、先頭行の画面行も運ぶ。動かな
// ければ欲しい列も保ち、$ の後は画面行の末尾へ着いたまま行末の欲しい列を保つ。
[[nodiscard]] VimStep row_navigated(const VimState &state, const VimEditorView &view,
                                    const VimDisplayNavigation &target, Offset placed)
{
    const Offset caret = rested_in(view.text, placed, state.mode);
    VimState next = navigated_state(state, view, caret);
    const bool line_end = state.wanted_column.has_value() &&
                          state.wanted_column.value().wish == VimColumnWish::at_line_end;
    if (caret == view.selection.caret || line_end)
    {
        next.wanted_column = state.wanted_column;
    }
    return VimStep{std::move(next), VimNavigate{navigated_selection(state, view.selection, caret),
                                                target.first, target.first_row}};
}

[[nodiscard]] VimStep displayed_navigation(const VimState &state, const VimEditorView &view,
                                           const VimDisplayNavigation &target)
{
    if (target.placed.has_value())
    {
        auto step = row_navigated(state, view, target, target.placed.value());
        if (target.scroll_lines.has_value())
        {
            step.next.scroll_lines = VimCount{target.scroll_lines.value()};
        }
        return step;
    }
    auto step = target.preserve_column
                    ? navigated_at(state, view, target.first, view.selection.caret)
                    : navigated(state, view, target.first, target.caret);
    if (target.scroll_lines.has_value())
    {
        step.next.scroll_lines = VimCount{target.scroll_lines.value()};
    }
    return step;
}

[[nodiscard]] VimStep half_page_step(const VimState &state, const VimEditorView &view,
                                     VimScrollDirection direction)
{
    if (view.navigation.has_value())
    {
        return displayed_navigation(state, view, view.navigation.value());
    }
    const std::size_t fallback =
        std::max<std::size_t>(vim_viewport_of(view).visible_lines / 2, single_step);
    const std::size_t height =
        std::max<std::size_t>(vim_viewport_of(view).visible_lines, single_step);
    const std::size_t requested =
        state.count.has_value()
            ? state.count.value().value
            : (state.scroll_lines.has_value() ? state.scroll_lines.value().value : fallback);
    const std::size_t configured = std::min(requested, height);
    const std::size_t amount = limited_amount(configured, view.text.line_count());
    const LineNumber requested_top = half_page_top(view, amount, direction);
    const LineNumber caret_line = shifted_line(line_of(view.text, view.selection.caret), amount,
                                               view.text.line_count(), direction);
    const LineNumber followed = first_visible_for_caret(
        requested_top, caret_line, vim_viewport_of(view).visible_lines, ScrollFollow::minimal);
    const LineNumber first =
        first_visible_within(followed, view.text.line_count(), vim_viewport_of(view).visible_lines,
                             ScrollExtent::last_line);
    const LineNumber current_line = line_of(view.text, view.selection.caret);
    VimStep step = caret_line == current_line
                       ? navigated_at(state, view, first, view.selection.caret)
                       : navigated(state, view, first, caret_line);
    if (state.count.has_value() &&
        (first != vim_viewport_of(view).first_visible || caret_line != current_line))
    {
        step.next.scroll_lines = VimCount{configured};
    }
    return step;
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep page_step(const VimState &state, const VimEditorView &view,
                                VimScrollDirection direction)
{
    if (view.navigation.has_value())
    {
        return displayed_navigation(state, view, view.navigation.value());
    }
    const LineNumber first = page_top(view, count_of(state.count), direction);
    if (first == vim_viewport_of(view).first_visible)
    {
        const Offset caret = view.selection.caret;
        return VimStep{navigated_state(state, view, caret), VimNavigate{view.selection, first}};
    }
    const std::size_t height =
        std::max<std::size_t>(vim_viewport_of(view).visible_lines, single_step);
    const std::size_t last =
        first.value + std::min(height - single_step, view.text.line_count() - first.value);
    LineNumber caret_line = first;
    switch (direction)
    {
    case VimScrollDirection::down:
        break;
    case VimScrollDirection::up:
        caret_line = LineNumber{
            std::min(last, previous_page_top(view, count_of(state.count)).value + single_step)};
        break;
    }
    if (!page_count_reached_the_end(view, count_of(state.count), direction))
    {
        return navigated(state, view, first, caret_line);
    }
    // 回数の途中で端に当たった周は Vim の onepage が FAIL を返し、beginline を飛ばす。キャレットは
    // 行だけ画面へ入り、同じバイトの桁を保つ（check_cursor_col・Issue #411）。
    const Offset caret = view.selection.caret;
    const std::size_t column = caret.value - view.text.line_start(line_of(view.text, caret)).value;
    return navigated_at(
        state, view, first,
        rested_in(view.text, vim_line_and_column(view.text, caret_line, column), state.mode));
}
} // namespace detail

namespace
{
// v / V で VISUAL に入る。回数があると入った直後にその広さぶんを選ぶ（Vim の nv_visual は
// count1 - 1 だけ v なら右へ・V なら下へ動かす。`3v` は 3 文字・`3V` は 3 行。Issue #53 で実測）。
[[nodiscard]] Offset widened(const TextBuffer &text, Offset caret, const VimState &state,
                             VimMode mode)
{
    const std::size_t steps = count_of(state.count) - single_step;
    switch (mode)
    {
    case VimMode::visual:
    case VimMode::visual_block:
        return forward_characters(text, caret, steps);
    case VimMode::visual_line:
    {
        const LineNumber line = line_of(text, caret);
        const LineNumber destination = line_below(text, line, steps);
        return destination == line
                   ? caret
                   : caret_on_line(text, wanted_column_of(text, state, caret), destination, mode);
    }
    case VimMode::normal:
    case VimMode::insert:
        return caret;
    }
    std::unreachable();
}

[[nodiscard]] VimStep entered_visual(const VimState &state, const TextBuffer &text, Offset caret,
                                     VimMode mode)
{
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.mode = mode;
    const Offset moved = widened(text, caret, state, mode);
    next.wanted_column = state.wanted_column;
    if ((mode == VimMode::visual || mode == VimMode::visual_block) && moved != caret)
    {
        next.wanted_column =
            wanted_after(wanted_column_of(text, state, caret),
                         landed_column(text, mode, caret, moved), VimMotion::right);
    }
    return VimStep{std::move(next), VimSelect{Selection{caret, moved}}};
}

[[nodiscard]] VimStep half_page_action(const VimState &state, const VimEditorView &view,
                                       VimAction action)
{
    const auto direction =
        action == VimAction::scroll_half_down ? VimScrollDirection::down : VimScrollDirection::up;
    return half_page_step(state, view, direction);
}

[[nodiscard]] VimStep page_action(const VimState &state, const VimEditorView &view,
                                  VimAction action)
{
    const auto direction =
        action == VimAction::scroll_page_down ? VimScrollDirection::down : VimScrollDirection::up;
    return page_step(state, view, direction);
}
} // namespace

namespace detail
{
// 半画面と 1 画面の巻き（ADR 0019）。NORMAL と VISUAL は同じ 1 本を通る。
[[nodiscard]] VimStep scroll_action(const VimState &state, const VimEditorView &view,
                                    VimAction action)
{
    if (action == VimAction::scroll_half_down || action == VimAction::scroll_half_up)
    {
        return half_page_action(state, view, action);
    }
    return page_action(state, view, action);
}
} // namespace detail

namespace
{
[[nodiscard]] VimStep visual_action(const VimState &state, const VimEditorView &view,
                                    VimAction action)
{
    const VimMode mode = action == VimAction::visual ? VimMode::visual : VimMode::visual_line;
    return entered_visual(state, view.text, view.selection.caret, mode);
}

[[nodiscard]] VimStep put_action(const VimState &state, const VimEditorView &view, VimAction action)
{
    const VimPutSide side = action == VimAction::put_after ? VimPutSide::after : VimPutSide::before;
    return put_step(state, view.text, view.selection.caret, side);
}

[[nodiscard]] VimStep line_end_action(const VimState &state, const VimEditorView &view,
                                      VimAction action)
{
    const VimOperator operation =
        action == VimAction::remove_to_line_end ? VimOperator::remove : VimOperator::change;
    return operated_to_line_end(state, view.text, view.selection.caret, operation);
}

[[nodiscard]] VimStep opened_command_line(const VimState &state)
{
    if (state.count.has_value())
    {
        return cancelled(state);
    }
    return VimStep{vim_resting_from(state, state.unnamed_register), VimOpenCommandLine{}};
}

[[nodiscard]] VimStep appended_insert(const VimState &state, const VimEditorView &view)
{
    return entered_insert(state, forward_characters(view.text, view.selection.caret, single_step));
}

[[nodiscard]] std::optional<VimStep>
character_search_action(const VimState &state, const VimEditorView &view, VimAction action)
{
    if (action == VimAction::find_character_forward)
    {
        return started_character_search(state, VimCharacterSearchKind::find_forward);
    }
    if (action == VimAction::find_character_backward)
    {
        return started_character_search(state, VimCharacterSearchKind::find_backward);
    }
    if (action == VimAction::till_character_forward)
    {
        return started_character_search(state, VimCharacterSearchKind::till_forward);
    }
    if (action == VimAction::till_character_backward)
    {
        return started_character_search(state, VimCharacterSearchKind::till_backward);
    }
    if (action == VimAction::repeat_character_search ||
        action == VimAction::repeat_character_search_opposite)
    {
        return repeated_character_search(state, view, action);
    }
    return std::nullopt;
}

[[nodiscard]] VimStep required_character_search_action(const VimState &state,
                                                       const VimEditorView &view, VimAction action)
{
    auto step = character_search_action(state, view, action);
    if (!step.has_value())
    {
        std::unreachable();
    }
    return std::move(step).value();
}

[[nodiscard]] VimStep pending_action(const VimState &state, VimAction action)
{
    const auto operation = vim_operator_for(action);
    if (!operation.has_value())
    {
        std::unreachable();
    }
    return pending_operator(state, operation.value());
}

[[nodiscard]] VimStep normal_visual_action(const VimState &state, const VimEditorView &view,
                                           VimAction action)
{
    if (action == VimAction::visual || action == VimAction::visual_line)
    {
        return visual_action(state, view, action);
    }
    if (action == VimAction::reselect_visual)
    {
        // `gv`（#1039）。前の選択が無ければ失敗する（Vim の nv_gv のビープ・実測）。
        auto step = reselected_visual(state, view);
        return step.has_value() ? std::move(step).value()
                                : failed(cancelled(state), VimRepeatFailure::not_found);
    }
    const LineNumber line = line_of(view.text, view.selection.caret);
    const bool below = action == VimAction::open_line_below;
    const Offset at = below ? view.text.line_end(line) : view.text.line_start(line);
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.mode = VimMode::insert;
    const std::size_t count = count_of(state.count);
    if (count > single_step)
    {
        next.insert_repeat =
            VimInsertRepeat{count - single_step, std::vector<VimKey>{VimKey{VimSpecialKey::enter}}};
    }
    return VimStep{
        std::move(next),
        VimInsertAt{at, "\n", Offset{at.value + (below ? single_step : 0)}, EditBoundary::absorb}};
}

// `I` の入る位置。最初の非空白、空白だけの行なら行末（空白の後ろ）。Vim の
// beginline(BL_WHITE | BL_FIX) で、`^` が空白だけの行で最後の文字に止まるのとは違う（#1027）。
[[nodiscard]] Offset insert_line_start_landing(const TextBuffer &text, Offset caret)
{
    const Offset landing = vim_first_non_blank(text, caret);
    const Offset end = text.line_end(line_of(text, caret));
    if (landing == end)
    {
        return landing;
    }
    const std::string first = text.text_range(landing, Offset{landing.value + 1});
    return first == " " || first == "\t" ? end : landing;
}

// `gi` の入る位置（#1039）。`'^` の行（本文の外なら最後の行）の桁で、桁は行の内容の終わりまで
// 寄せる（Vim の nv_g_cmd の check_cursor_lnum と行の長さへの切り詰め）。`'^` が無ければ
// キャレットの所（`i` と同じ・実測）。
[[nodiscard]] Offset last_insert_landing(const VimEditorView &view)
{
    const std::optional<VimMarkPosition> &mark = view.marks->last_insert;
    if (!mark.has_value())
    {
        return view.selection.caret;
    }
    const LineNumber line{std::min(mark.value().line.value, view.text.line_count())};
    return vim_line_and_column(view.text, line, mark.value().byte);
}

[[nodiscard]] VimStep insert_action(const VimState &state, const VimEditorView &view,
                                    VimAction action)
{
    if (action == VimAction::insert_before)
    {
        return entered_insert(state, view.selection.caret);
    }
    if (action == VimAction::insert_at_last_insert)
    {
        return entered_insert(state, last_insert_landing(view));
    }
    if (action == VimAction::insert_after)
    {
        return appended_insert(state, view);
    }
    if (action == VimAction::insert_at_line_start)
    {
        return entered_insert(state, insert_line_start_landing(view.text, view.selection.caret));
    }
    return entered_insert(state, view.text.line_end(line_of(view.text, view.selection.caret)));
}

// u / Ctrl-r / `.`。どれも済んだ編集をもう一度たどる（ADR 0030 の決定 6）。
[[nodiscard]] VimStep history_action(const VimState &state, VimAction action)
{
    if (action == VimAction::undo)
    {
        return VimStep{vim_resting_from(state, state.unnamed_register), VimUndo{}};
    }
    if (action == VimAction::redo)
    {
        return VimStep{vim_resting_from(state, state.unnamed_register), VimRedo{}};
    }
    if (action == VimAction::undo_line)
    {
        // NORMAL の `U`（行の undo）は未実装。表に無い鍵と同じに打ち消す（ADR 0015 の決定 10）。
        return cancelled(state);
    }
    return repeated_change(state);
}

// 範囲に効く本文の変更（x ~ と保留を立てるオペレータの鍵）。
[[nodiscard]] VimStep normal_range_action(const VimState &state, const VimEditorView &view,
                                          VimAction action)
{
    if (action == VimAction::remove_character)
    {
        return removed_character(state, view.text, view.selection.caret);
    }
    if (action == VimAction::toggle_case_character)
    {
        return swapped_characters(state, view.text, view.selection.caret);
    }
    return pending_action(state, action);
}

// 行に効く本文の変更（X p P D C Y J gJ）。
[[nodiscard]] VimStep normal_line_action(const VimState &state, const VimEditorView &view,
                                         VimAction action)
{
    if (action == VimAction::remove_character_before)
    {
        return removed_character_before(state, view);
    }
    if (action == VimAction::join_lines || action == VimAction::join_lines_raw)
    {
        return joined(state, view, action == VimAction::join_lines);
    }
    if (action == VimAction::put_after || action == VimAction::put_before)
    {
        return put_action(state, view, action);
    }
    if (action == VimAction::remove_to_line_end || action == VimAction::change_to_line_end)
    {
        return line_end_action(state, view, action);
    }
    if (action == VimAction::yank_line)
    {
        return operated_on_lines(state, view.text, view.selection.caret, VimOperator::yank);
    }
    std::unreachable();
}

// 次の 1 鍵を待つ接頭辞の動作 → 接頭辞（g z @ " と m ' `・ADR 0152 の決定 3）。ほかは nullopt。
[[nodiscard]] std::optional<VimPrefix> prefix_of(VimAction action) noexcept
{
    constexpr std::array<std::pair<VimAction, VimPrefix>, 7> prefixes{
        {{VimAction::prefix_g, VimPrefix::g},
         {VimAction::prefix_z, VimPrefix::z},
         {VimAction::replay_macro, VimPrefix::at},
         {VimAction::select_register, VimPrefix::quote},
         {VimAction::set_mark, VimPrefix::mark},
         {VimAction::prefix_mark_line, VimPrefix::mark_line},
         {VimAction::prefix_mark_exact, VimPrefix::mark_exact}}};
    for (const auto &[from, prefix] : prefixes)
    {
        if (from == action)
        {
            return prefix;
        }
    }
    return std::nullopt;
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep input_action(const VimState &state, const VimEditorView &view,
                                   VimAction action)
{
    // VISUAL の i / a はテキストオブジェクトの接頭辞（ADR 0031 の決定 1）。NORMAL の挿入命令は
    // commanded が先に insert_action へ分けるので、ここへは VISUAL からしか来ない。
    if (action == VimAction::insert_before || action == VimAction::insert_after)
    {
        return text_object_action(state, action);
    }
    if (action == VimAction::open_command_line)
    {
        return opened_command_line(state);
    }
    if (const auto prefix = prefix_of(action))
    {
        return started_prefix(state, prefix.value());
    }
    if (action == VimAction::replace_character)
    {
        return started_replacement(state);
    }
    if (action == VimAction::record_macro)
    {
        return macro_record_action(state, view);
    }
    return required_character_search_action(state, view, action);
}

// `gt` `gT`（ADR 0057 の決定 2）。行き先は tab_destination の 1 本が決め、値なしなら
// refused に not_moved の印を添えて返す（タブは動かず、再生の中なら残りの鍵が捨てられる）。
// 回数は `{N}gt` の N で、無いときは隣へ。切り替えが VISUAL を終えるので、成功は NORMAL の
// 休止で返す（ADR 0056 の決定 4）。
[[nodiscard]] VimStep tab_action(const VimState &state, const VimEditorView &view, VimAction action,
                                 VimStep refused)
{
    const TabJumpDirection direction =
        action == VimAction::next_tab ? TabJumpDirection::forward : TabJumpDirection::backward;
    const std::optional<std::size_t> count =
        state.count.has_value() ? std::optional<std::size_t>{state.count.value().value}
                                : std::nullopt;
    const auto destination =
        tab_destination(TabJump{direction, count}, view.tabs.active, view.tabs.count);
    if (!destination.has_value())
    {
        return failed(std::move(refused), VimRepeatFailure::not_moved);
    }
    return VimStep{vim_resting_from(state, state.unnamed_register),
                   VimSwitchTab{destination.value()}};
}

// 足りない文字組みの値を、効果を出さず状態もそのままで求める（ADR 0144 の決定 2）。行の画面行数か
// 行の中の画面行のどちらか 1 つで、controller が答えて同じ鍵をもう一度流す。
[[nodiscard]] VimStep requested_layout(const VimState &state, const VimLayoutNeed &need)
{
    auto step = VimStep{state, VimNoEffect{}};
    if (const auto *const line = std::get_if<LineNumber>(&need); line != nullptr)
    {
        step.layout_request = *line;
        return step;
    }
    step.row_request = std::get<VimRowRequest>(need);
    return step;
}

// 窓と wrap off の横送り（ADR 0145 の決定 5〜7）。横に送らない文書では何もしない（失敗にしない・
// 欲しい列も保つ）。押したときだけ欲しい列を押された位置の桁に置き、先頭行と先頭の画面行は今の値の
// まま、新しい leftcol を first_column で返す。NORMAL と VISUAL は同じ 1 本を通る。
[[nodiscard]] VimStep horizontal_action(const VimState &state, const VimEditorView &view,
                                        VimAction action)
{
    const auto scrolled = vim_horizontal_scroll(state, view, action);
    if (!scrolled)
    {
        return requested_layout(state, scrolled.error());
    }
    VimState next = finished_input_wait(state);
    const std::optional<VimHorizontalScroll> &outcome = scrolled.value();
    if (!outcome.has_value())
    {
        return VimStep{std::move(next), VimNoEffect{}};
    }
    const VimHorizontalScroll &result = outcome.value();
    if (result.pushed)
    {
        next.wanted_column = VimWantedColumn{
            VimColumnWish::at_column,
            landed_column(view.text, state.mode, view.selection.anchor, result.caret)};
    }
    const VimViewport viewport = vim_viewport_of(view);
    return VimStep{std::move(next),
                   VimNavigate{navigated_selection(state, view.selection, result.caret),
                               viewport.first_visible, viewport.first_row, result.left_column}};
}
} // namespace detail

namespace
{
// 縦の位置合わせの行の選択（ADR 0146 の決定 3）。回数があれば min(回数, 行数) 行目の同じバイトの桁
// （行の最後の文字の先頭に収める・Vim の check_cursor_col）、無ければ今のキャレット。
[[nodiscard]] Offset counted_line_caret(const VimState &state, const VimEditorView &view)
{
    const Offset caret = view.selection.caret;
    if (!state.count.has_value())
    {
        return caret;
    }
    const TextBuffer &text = view.text;
    const LineNumber current = text.position_of(caret).line;
    const LineNumber line{std::clamp<std::size_t>(state.count.value().value, 1, text.line_count())};
    const std::size_t column = caret.value - text.line_start(current).value;
    return rested_in(text, vim_line_and_column(text, line, column), state.mode);
}

// z^ の回数つきの行（ADR 0146 の決定 9・Vim の nv_zet）。回数の行を下端に置いたときの先頭の行。
[[nodiscard]] std::expected<LineNumber, VimLayoutNeed> counted_above_line(const VimState &state,
                                                                          const VimEditorView &view)
{
    const auto top = vim_aligned_top(view, counted_line_caret(state, view), VimScreenPlace::bottom);
    if (!top)
    {
        return std::unexpected(top.error());
    }
    return top.value().line;
}

// 選んだ行の行頭（呼び手が最初の非空白へ動かす）。行が決まらなければ要る配置を返す。
[[nodiscard]] std::expected<Offset, VimLayoutNeed>
line_start_of(const VimEditorView &view, const std::expected<LineNumber, VimLayoutNeed> &line)
{
    if (!line)
    {
        return std::unexpected(line.error());
    }
    return view.text.line_start(line.value());
}

// 合わせる行のキャレット（ADR 0146 の決定 3・9）。caret と回数つきの z+ は回数の行か今の行
// （counted_line_caret）。z+ z^ の回数なしは窓の 1 つ下・上の行、回数つきの z^ は回数の行を下端に
// 置いたときの先頭の行で、どちらも行頭（line_start_of）。
[[nodiscard]] std::expected<Offset, VimLayoutNeed>
aligned_line_caret(const VimState &state, const VimEditorView &view, VimAlignedLine choice)
{
    const bool counted = state.count.has_value();
    switch (choice)
    {
    case VimAlignedLine::caret:
        return counted_line_caret(state, view);
    case VimAlignedLine::below_window:
        return counted ? std::expected<Offset, VimLayoutNeed>{counted_line_caret(state, view)}
                       : line_start_of(view, vim_line_below_window(view));
    case VimAlignedLine::above_window:
        return counted ? line_start_of(view, counted_above_line(state, view))
                       : std::expected<Offset, VimLayoutNeed>{
                             view.text.line_start(vim_line_above_window(view))};
    }
    std::unreachable();
}
} // namespace

namespace detail
{
// 縦の位置合わせ zt zz zb z<CR> z. z- z+ z^（ADR 0146 の決定 1〜6・9）。回数か窓の外の行で行を
// 選び、z<CR> z. z- z+ z^ は行の最初の非空白へ動いて（欲しい列もそこ）から、engine が先頭行を
// 決めて VimNavigate で返す。
// zt zz zb は欲しい列を保つ。横位置は保つ（first_column は無し）。行が画面より高いときの先頭は
// 決定 5 の規則。NORMAL と VISUAL は同じ 1 本を通り、VISUAL は錨を保つ。
[[nodiscard]] VimStep aligned_screen(const VimState &state, const VimEditorView &view,
                                     VimAction action)
{
    const auto alignment = vim_screen_alignment_for(action);
    if (!alignment.has_value())
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    const bool first_non_blank = alignment.value().first_non_blank;
    const auto chosen = aligned_line_caret(state, view, alignment.value().line);
    if (!chosen)
    {
        return requested_layout(state, chosen.error());
    }
    const Offset caret =
        first_non_blank ? vim_first_non_blank(view.text, chosen.value()) : chosen.value();
    const auto top = vim_aligned_top(view, caret, alignment.value().place);
    if (!top)
    {
        return requested_layout(state, top.error());
    }
    const VisualPosition first = top.value();
    VimState next = navigated_state(finished_input_wait(state), view, caret);
    if (!first_non_blank)
    {
        next.wanted_column = state.wanted_column;
    }
    return VimStep{std::move(next), VimNavigate{navigated_selection(state, view.selection, caret),
                                                first.line, first.row}};
}
} // namespace detail

namespace
{
// 鍵から引いた動作を、分類ごとの写し先へ（CPP-012 / ADR 0006）。分類が増えたらここで落ちる。
[[nodiscard]] VimStep commanded(const VimState &state, const VimEditorView &view, VimAction action)
{
    const auto group = vim_group_for(action);
    if (!group.has_value())
    {
        return cancelled(state);
    }
    switch (group.value())
    {
    case VimActionGroup::motion:
        return moved_step(state, view, action);
    case VimActionGroup::scroll:
        return scroll_action(state, view, action);
    case VimActionGroup::enter_visual:
        return normal_visual_action(state, view, action);
    case VimActionGroup::edit_range:
        return normal_range_action(state, view, action);
    case VimActionGroup::edit_line:
        return normal_line_action(state, view, action);
    case VimActionGroup::insert_object:
    case VimActionGroup::insert_line:
        return insert_action(state, view, action);
    case VimActionGroup::history:
        return history_action(state, action);
    case VimActionGroup::input_wait:
    case VimActionGroup::ex_line:
        return input_action(state, view, action);
    case VimActionGroup::search:
        return search_action(state, view, action);
    case VimActionGroup::tab:
        return tab_action(state, view, action, cancelled(state));
    case VimActionGroup::horizontal_scroll:
        return horizontal_action(state, view, action);
    case VimActionGroup::screen_alignment:
        return aligned_screen(state, view, action);
    case VimActionGroup::number:
        return numbered(state, view, action);
    case VimActionGroup::jump_list:
        return jump_list_action(state, view, action);
    }
    std::unreachable();
}

// 保留中のオペレータの後ろ。同じ鍵なら行単位、移動なら範囲、ほかの鍵なら打ち消し（Vim と同じ）。
[[nodiscard]] VimStep pending_step(const VimState &state, const VimEditorView &view,
                                   VimAction action)
{
    const VimOperator operation = operation_of(state);
    // z も次の 1 鍵まで読み、横送りの鍵ならオペレータを打ち消す（Vim の nv_zet の checkclearop）。
    // `'` `` ` `` も次の鍵（マークの名前）まで読む（ADR 0152 の決定 3）。`m` は Vim の nv_mark
    // と同じに オペレータを打ち消す（下の移動にならない鍵の道）。
    if (action == VimAction::prefix_g || action == VimAction::prefix_z ||
        action == VimAction::prefix_mark_line || action == VimAction::prefix_mark_exact)
    {
        return started_prefix(state, prefix_of(action).value_or(VimPrefix::g));
    }
    if (action == VimAction::insert_before || action == VimAction::insert_after)
    {
        return text_object_action(state, action);
    }
    // `d<C-a>` はオペレータを打ち消すだけで失敗にしない（Vim の nv_addsub の clearop・ADR 0015 の
    // 決定 14）。
    if (vim_group_for(action) == VimActionGroup::number)
    {
        return cancelled(state);
    }
    // 同じオペレータの鍵をもう一度押すと行単位（dd cc yy g~~ guu gUU >> <<・表は VimKeyTable）。
    if (vim_operator_doubles(operation, action))
    {
        return operated_on_lines(state, view.text, view.selection.caret, operation);
    }
    const auto search_step = character_search_action(state, view, action);
    if (search_step.has_value())
    {
        return search_step.value();
    }
    // 検索の 6 つの鍵は保留中のオペレータの範囲を作る（ADR 0032 の決定 3）。
    if (vim_group_for(action) == VimActionGroup::search)
    {
        return search_action(state, view, action);
    }
    const auto motion = laid_out_motion(view, action);
    if (!motion.has_value())
    {
        return failed(cancelled(state), VimRepeatFailure::refused);
    }
    return operated(state, view, motion.value());
}

// gj gk の向き。ほかの動作は無し。
[[nodiscard]] std::optional<VimScrollDirection> row_direction(VimAction action) noexcept
{
    if (action == VimAction::move_display_down)
    {
        return VimScrollDirection::down;
    }
    return action == VimAction::move_display_up ? std::optional{VimScrollDirection::up}
                                                : std::nullopt;
}

// g0 gm g$ g^ の所。ほかの動作は無し（ADR 0147 の決定 1・8）。
[[nodiscard]] std::optional<VimDisplayLineEdge> display_line_edge(VimAction action) noexcept
{
    if (action == VimAction::move_display_line_start)
    {
        return VimDisplayLineEdge::start;
    }
    if (action == VimAction::move_display_line_middle)
    {
        return VimDisplayLineEdge::middle;
    }
    if (action == VimAction::move_display_line_first_non_blank)
    {
        return VimDisplayLineEdge::first_non_blank;
    }
    return action == VimAction::move_display_line_end ? std::optional{VimDisplayLineEdge::end}
                                                      : std::nullopt;
}

// `gM` の着地（ADR 0147 の決定 2・3）。行の本文の幅（行頭からの仮想桁の全体・索引があれば O(塊)）
// の半分の升を含む文字。回数 1〜100 は幅 × N / 100 の升（100gM は最後の文字）、100 超は無視して
// 半分。行より右なら行の内容の終わり（モードの寄せは呼び手・Vim の coladvance）。欲しい列は
// 着地の桁。
[[nodiscard]] VimRowLanding line_middle_landing(const VimState &state, const VimEditorView &view)
{
    constexpr std::size_t percent = 100;
    const TextBuffer &text = view.text;
    const LineNumber line = line_of(text, view.selection.caret);
    const std::size_t width = virtual_column(text, text.line_end(line)).value - 1;
    const auto count = combined_count(state);
    const bool scaled = count.has_value() && count.value().value <= percent;
    const std::size_t cell = scaled ? width * count.value().value / percent : width / 2;
    const Offset at = offset_at_virtual_column(text, line, VirtualColumn{cell + 1});
    return VimRowLanding{VimMotionLanding{at},
                         VimWantedColumn{VimColumnWish::at_column, caret_virtual_column(text, at)}};
}

// g0 gm g$ gM の着地（ADR 0147 の決定 2）。表示領域と画面行の範囲から解き、ほかの動作は無し。
[[nodiscard]] std::expected<std::optional<VimRowLanding>, VimLayoutNeed>
display_line_view(const VimState &state, const VimEditorView &view, VimAction action)
{
    if (action == VimAction::move_line_middle)
    {
        return std::optional{line_middle_landing(state, view)};
    }
    const auto edge = display_line_edge(action);
    if (!edge.has_value())
    {
        return std::nullopt;
    }
    const auto landing = vim_display_line_landing(view, edge.value(), resolved_count(state));
    if (!landing)
    {
        return std::unexpected(landing.error());
    }
    return std::optional{landing.value()};
}
} // namespace

namespace detail
{
// 文字組みの値で解く動作を、鍵を写す前に解いて view に置く。画面移動（ADR 0019）と、文字組みが
// 有効なときの gj gk の着地（ADR 0144 の決定 4・回数はオペレータ側と移動側の積）と、g0 g$ gm gM の
// 着地（ADR 0147 の決定 2）。
[[nodiscard]] std::expected<VimEditorView, VimLayoutNeed>
display_view(const VimState &state, const VimEditorView &view, VimAction action)
{
    auto navigation = vim_display_navigation(state, view, action);
    if (!navigation)
    {
        return std::unexpected(navigation.error());
    }
    auto line_landing = display_line_view(state, view, action);
    if (!line_landing)
    {
        return std::unexpected(line_landing.error());
    }
    auto result = view;
    result.navigation = std::move(navigation).value();
    if (line_landing.value().has_value())
    {
        result.row_landing = std::move(line_landing).value();
    }
    const auto direction = row_direction(action);
    if (!direction.has_value() || !view.layout.enabled)
    {
        return result;
    }
    auto landing =
        vim_row_landing(view, direction.value(), resolved_count(state), state.wanted_column);
    if (!landing)
    {
        return std::unexpected(landing.error());
    }
    result.row_landing = landing.value();
    return result;
}

// 画面を読む鍵に表示領域が無ければ、効果を出さず状態もそのままで求める（ADR 0129 の決定 1）。
// 回数や保留中のオペレータも消費しない。controller が渡して同じ鍵をもう一度流す。
[[nodiscard]] std::optional<VimStep> requested_viewport(const VimState &state,
                                                        const VimEditorView &view, VimAction action)
{
    if (view.viewport.has_value() || !vim_reads_viewport(action))
    {
        return std::nullopt;
    }
    auto step = VimStep{state, VimNoEffect{}};
    step.viewport_request = true;
    return step;
}
} // namespace detail

namespace
{
[[nodiscard]] VimStep acted(const VimState &state, const VimEditorView &input, VimAction action)
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
    if (state.pending.has_value())
    {
        return pending_step(state, view, action);
    }
    return commanded(state, view, action);
}
} // namespace

namespace detail
{
[[nodiscard]] bool counts_as_digit(const VimState &state, char32_t key) noexcept
{
    if (key < U'0' || key > U'9')
    {
        return false;
    }
    return key != U'0' || state.count.has_value();
}

[[nodiscard]] VimStep counted(const VimState &state, char32_t key)
{
    const auto digit = static_cast<std::size_t>(key - U'0');
    const std::size_t previous = state.count.has_value() ? state.count.value().value : 0;
    const std::size_t largest = std::numeric_limits<std::size_t>::max();
    const std::size_t carried =
        previous > (largest - digit) / decimal_base ? largest - digit : previous * decimal_base;
    VimState next = state;
    next.count = VimCount{carried + digit};
    return VimStep{std::move(next), VimNoEffect{}};
}
} // namespace detail

namespace
{
[[nodiscard]] VimStep normal_character(const VimState &state, const VimEditorView &view,
                                       VimCharacter key)
{
    if (counts_as_digit(state, key.code))
    {
        return counted(state, key.code);
    }
    const auto action = vim_action_for(key.code);
    if (!action.has_value())
    {
        return failed(cancelled(state), VimRepeatFailure::refused);
    }
    return acted(state, view, action.value());
}
} // namespace

namespace detail
{
// `Ctrl-w`（ADR 0148 の決定 7）。NORMAL と VISUAL で次の 1 鍵を窓の命令として待つ。オペレータの
// 後ろでは Vim と同じく失敗してオペレータを打ち消す。回数は待ちのあいだ持ち、2 打鍵目で捨てる。
[[nodiscard]] VimStep window_started(const VimState &state)
{
    if (state.pending.has_value())
    {
        return failed(cancelled(state), VimRepeatFailure::refused);
    }
    return started_prefix(state, VimPrefix::window_command);
}
} // namespace detail

namespace
{
// NORMAL の特別な鍵。矢印は h j k l と同じ動作で、Home / End は 0 と $（ADR 0015 の決定 6）。
// Esc は保留を捨てるだけ（ADR 0012 の決定 5）。Backspace は行をまたぐ h（ADR 0049 の決定 1）。
[[nodiscard]] VimStep normal_special(const VimState &state, const VimEditorView &view,
                                     VimSpecialKey key)
{
    switch (key)
    {
    case VimSpecialKey::escape:
        return cancelled(state);
    case VimSpecialKey::backspace:
        return acted(state, view, VimAction::space_left);
    case VimSpecialKey::enter:
        return acted(state, view, VimAction::move_next_line);
    case VimSpecialKey::arrow_left:
        return acted(state, view, VimAction::move_left);
    case VimSpecialKey::arrow_right:
        return acted(state, view, VimAction::move_right);
    case VimSpecialKey::arrow_up:
        return acted(state, view, VimAction::move_up);
    case VimSpecialKey::arrow_down:
        return acted(state, view, VimAction::move_down);
    case VimSpecialKey::home:
        return acted(state, view, VimAction::move_line_start);
    case VimSpecialKey::end:
        return acted(state, view, VimAction::move_line_end);
    case VimSpecialKey::control_r:
        return acted(state, view, VimAction::redo);
    case VimSpecialKey::page_up:
    case VimSpecialKey::control_b:
        return acted(state, view, VimAction::scroll_page_up);
    case VimSpecialKey::page_down:
    case VimSpecialKey::control_f:
        return acted(state, view, VimAction::scroll_page_down);
    case VimSpecialKey::control_d:
        return acted(state, view, VimAction::scroll_half_down);
    case VimSpecialKey::control_u:
        return acted(state, view, VimAction::scroll_half_up);
    case VimSpecialKey::control_v:
        return entered_visual(state, view.text, view.selection.caret, VimMode::visual_block);
    case VimSpecialKey::control_w:
        return window_started(state);
    case VimSpecialKey::control_a:
        return acted(state, view, VimAction::increment_number);
    case VimSpecialKey::control_x:
        return acted(state, view, VimAction::decrement_number);
    case VimSpecialKey::control_o:
        return acted(state, view, VimAction::jump_older);
    }
    std::unreachable();
}

[[nodiscard]] std::optional<char32_t> character_search_target(VimSpecialKey key) noexcept
{
    switch (key)
    {
    case VimSpecialKey::enter:
        return carriage_return_character;
    case VimSpecialKey::control_r:
        return control_r_character;
    case VimSpecialKey::control_d:
        return control_d_character;
    case VimSpecialKey::control_u:
        return control_u_character;
    case VimSpecialKey::control_f:
        return control_f_character;
    case VimSpecialKey::control_b:
        return control_b_character;
    case VimSpecialKey::control_w:
        return vim_window_control_character(key);
    case VimSpecialKey::control_a:
    case VimSpecialKey::control_x:
        return vim_number_control_character(key);
    case VimSpecialKey::escape:
    case VimSpecialKey::backspace:
    case VimSpecialKey::arrow_left:
    case VimSpecialKey::arrow_right:
    case VimSpecialKey::arrow_up:
    case VimSpecialKey::arrow_down:
    case VimSpecialKey::home:
    case VimSpecialKey::end:
    case VimSpecialKey::page_up:
    case VimSpecialKey::page_down:
    case VimSpecialKey::control_v:
    case VimSpecialKey::control_o:
        return std::nullopt;
    }
    std::unreachable();
}

[[nodiscard]] VimStep awaited_character(const VimState &state, const VimEditorView &view,
                                        VimCharacterSearchKind kind, char32_t target)
{
    const VimCharacterSearch search{kind, target};
    return completed_character_search(state, view, search, VimCharacterSearchInvocation::first);
}

[[nodiscard]] VimStep awaited_step(const VimState &state, const VimEditorView &view,
                                   VimCharacterSearchKind kind, VimKey key)
{
    if (std::holds_alternative<VimCharacter>(key))
    {
        return awaited_character(state, view, kind, std::get<VimCharacter>(key).code);
    }
    if (!std::holds_alternative<VimSpecialKey>(key))
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    const VimSpecialKey special = std::get<VimSpecialKey>(key);
    const auto target = character_search_target(special);
    if (!target.has_value())
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    return awaited_character(state, view, kind, target.value());
}

[[nodiscard]] VimStep awaited_step(const VimState &state, const VimEditorView &view,
                                   VimTextObjectScope scope, VimKey key)
{
    if (!std::holds_alternative<VimCharacter>(key))
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    const auto object = vim_text_object_for(std::get<VimCharacter>(key).code);
    if (!object.has_value())
    {
        return failed(VimStep{finished_input_wait(state), VimNoEffect{}},
                      VimRepeatFailure::refused);
    }
    const VimTextObjectOutcome outcome = word_writing_empty(
        vim_text_object_range(view.text, view.selection,
                              VimTextObjectRequest{scope, object.value(), state.mode},
                              resolved_count(state)),
        object.value());
    // 答えの種類が増えたら写し先が足りずコンパイルが落ちる（CPP-002）。範囲にならなかったときは
    // キャレットが動いても Vim はビープする（ADR 0046 の決定 3）。
    VimStep step = std::visit([&](const auto &value)
                              { return completed_text_object(state, view, value); }, outcome);
    if (std::holds_alternative<VimTextObjectCancel>(outcome))
    {
        return failed(std::move(step), VimRepeatFailure::not_found);
    }
    return step;
}

[[nodiscard]] VimStep completed_prefix(const VimState &state, const VimEditorView &view,
                                       VimAction action)
{
    switch (state.mode)
    {
    case VimMode::normal:
        return acted(state, view, action);
    case VimMode::insert:
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return visual_acted(state, view, action);
    }
    std::unreachable();
}

// g の後ろの鍵を表の文字へ。Ctrl-a と Ctrl-x は 0x01 と 0x18（`g Ctrl-a`・ADR 0015 の決定 14）。
// ほかの特別な鍵と検索の文字列は表に無い。
[[nodiscard]] std::optional<char32_t> g_key_code(const VimKey &key) noexcept
{
    if (const auto *const character = std::get_if<VimCharacter>(&key); character != nullptr)
    {
        return character->code;
    }
    const auto *const special = std::get_if<VimSpecialKey>(&key);
    return special != nullptr ? vim_number_control_character(*special) : std::nullopt;
}

// g の後ろの鍵（gg ge gE）。表に無い鍵は g を打ち消す（Issue #222）。
[[nodiscard]] VimStep g_prefix_step(const VimState &state, const VimEditorView &view, VimKey key)
{
    const std::optional<VimAction> action =
        g_key_code(key).and_then([](char32_t code) { return vim_g_action_for(code); });
    if (action.has_value())
    {
        return completed_prefix(state, view, action.value());
    }
    return VimStep{finished_input_wait(state), VimNoEffect{}};
}

// z の後ろの鍵を表の文字へ。<CR> は 0x0D（z<CR>）。ほかの特別な鍵と検索の文字列は表に無い 0。
[[nodiscard]] char32_t z_key_code(const VimKey &key) noexcept
{
    if (const auto *const character = std::get_if<VimCharacter>(&key); character != nullptr)
    {
        return character->code;
    }
    const auto *const special = std::get_if<VimSpecialKey>(&key);
    return special != nullptr && *special == VimSpecialKey::enter ? carriage_return_character
                                                                  : char32_t{0};
}

// z の後ろの鍵（zl zh zL zH zs ze・ADR 0145 の決定 5。zt zz zb z<CR> z. z-・ADR 0146）。Esc は
// z を打ち消すだけ。表に無い鍵は Vim の nv_zet と同じく失敗して後ろを打ち切る。
[[nodiscard]] VimStep z_prefix_step(const VimState &state, const VimEditorView &view, VimKey key)
{
    if (const auto *const special = std::get_if<VimSpecialKey>(&key);
        special != nullptr && *special == VimSpecialKey::escape)
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    const std::optional<VimAction> action = vim_z_action_for(z_key_code(key));
    if (action.has_value())
    {
        return completed_prefix(state, view, action.value());
    }
    return failed(VimStep{finished_input_wait(state), VimNoEffect{}}, VimRepeatFailure::refused);
}

// `Ctrl-w` の後ろの鍵 → 窓の命令。文字と特別な鍵は VimKeyTable の表の口を引き、検索の文字列は
// 表に無い。
[[nodiscard]] std::optional<VimWindowCommand> window_command_of(const VimKey &key) noexcept
{
    if (const auto *const character = std::get_if<VimCharacter>(&key); character != nullptr)
    {
        return vim_window_command_for(character->code);
    }
    const auto *const special = std::get_if<VimSpecialKey>(&key);
    return special != nullptr ? vim_window_command_for(*special) : std::nullopt;
}

// `Ctrl-w` の後ろの鍵（ADR 0148 の決定 7）。窓の命令を効果として返すだけで、モードと選択は
// 変えない（VISUAL のままで、枠を移るときに畳むのは controller）。回数は捨てる。Esc は待ちを
// 消すだけ。表に無い鍵は失敗して後ろを打ち切る。
[[nodiscard]] VimStep window_step(const VimState &state, VimKey key)
{
    if (const auto *const special = std::get_if<VimSpecialKey>(&key);
        special != nullptr && *special == VimSpecialKey::escape)
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    const std::optional<VimWindowCommand> command = window_command_of(key);
    if (!command.has_value())
    {
        return failed(VimStep{finished_input_wait(state), VimNoEffect{}},
                      VimRepeatFailure::refused);
    }
    return VimStep{finished_input_wait(state), command.value()};
}

[[nodiscard]] bool is_escape(const VimKey &key) noexcept
{
    const auto *const special = std::get_if<VimSpecialKey>(&key);
    return special != nullptr && *special == VimSpecialKey::escape;
}

// 名前の鍵にならなかったとき。Esc は何もせずに打ち消し、ほかは受けない（Vim の `m<Esc>`
// `'<Esc>`）。
[[nodiscard]] VimStep unnamed_mark(const VimState &state, const VimKey &key)
{
    VimStep step{finished_input_wait(state), VimNoEffect{}};
    return is_escape(key) ? step : failed(std::move(step), VimRepeatFailure::refused);
}

// `m{name}`（ADR 0152 の決定 3）。今のキャレットを置いた新しい値を添え、本文も選択も動かさない
// （VISUAL のまま）。オペレータの後ろへは来ない（pending_step が打ち消す）。
[[nodiscard]] VimStep mark_set_step(const VimState &state, const VimEditorView &view,
                                    const VimKey &key)
{
    const auto *const character = std::get_if<VimCharacter>(&key);
    if (character == nullptr)
    {
        return unnamed_mark(state, key);
    }
    VimStep step{finished_input_wait(state), VimNoEffect{}};
    const auto marks = vim_mark_set(*view.marks, character->code,
                                    vim_mark_position_of(view.text, view.selection.caret));
    if (!marks)
    {
        return failed(std::move(step), marks.error());
    }
    step.marks = marks.value();
    return step;
}

// `'{name}` `` `{name} ``（決定 3）。名前を解いて着地を view に置き、移動の表の 1 本
// （mark_line / mark_exact）へ写す。置いていないマークは E20、本文の外の行は E19 の失敗。
[[nodiscard]] VimStep mark_jump_step(const VimState &state, const VimEditorView &view,
                                     VimPrefix prefix, const VimKey &key)
{
    const auto *const character = std::get_if<VimCharacter>(&key);
    if (character == nullptr)
    {
        return unnamed_mark(state, key);
    }
    VimStep unmoved{finished_input_wait(state), VimNoEffect{}};
    const auto mark = vim_mark_named(*view.marks, character->code);
    if (!mark)
    {
        return failed(std::move(unmoved), mark.error());
    }
    const auto landing = vim_mark_offset(view.text, mark.value());
    if (!landing.has_value())
    {
        return failed(std::move(unmoved), VimRepeatFailure::not_found);
    }
    VimEditorView marked = view;
    marked.mark_landing = landing.value();
    return completed_prefix(state, marked,
                            prefix == VimPrefix::mark_line ? VimAction::move_mark_line
                                                           : VimAction::move_mark_exact);
}

[[nodiscard]] VimStep awaited_step(const VimState &state, const VimEditorView &view,
                                   VimPrefix prefix, VimKey key)
{
    switch (prefix)
    {
    case VimPrefix::g:
        return g_prefix_step(state, view, key);
    case VimPrefix::r:
        return replacement_key(state, view, key);
    case VimPrefix::q:
        return started_macro(state, key);
    case VimPrefix::at:
        return replayed_macro(state, key);
    case VimPrefix::quote:
        return selected_register(state, key);
    case VimPrefix::z:
        return z_prefix_step(state, view, key);
    case VimPrefix::window_command:
        return window_step(state, key);
    case VimPrefix::mark:
        return mark_set_step(state, view, key);
    case VimPrefix::mark_line:
    case VimPrefix::mark_exact:
        return mark_jump_step(state, view, prefix, key);
    case VimPrefix::insert_register:
        return insert_register_step(state, view, key);
    }
    std::unreachable();
}

[[nodiscard]] VimStep awaiting_step(const VimState &state, const VimEditorView &view, VimKey key)
{
    if (!state.input_wait.has_value())
    {
        std::unreachable();
    }
    return std::visit([&](const auto wait) { return awaited_step(state, view, wait, key); },
                      state.input_wait.value());
}

// 鍵の種類とモードの組（ADR 0012 の決定 4 / ADR 0032 の決定 3）。鍵の種類が増えたら
// std::visit の写し先が足りずコンパイルが落ちる（CPP-002）。
[[nodiscard]] VimStep pressed(const VimState &state, const VimEditorView &view, VimCharacter key)
{
    switch (state.mode)
    {
    case VimMode::normal:
        return normal_character(state, view, key);
    case VimMode::insert:
        return insert_character(state, key);
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return visual_character(state, view, key);
    }
    std::unreachable();
}

[[nodiscard]] VimStep pressed(const VimState &state, const VimEditorView &view, VimSpecialKey key)
{
    switch (state.mode)
    {
    case VimMode::normal:
        return normal_special(state, view, key);
    case VimMode::insert:
        return insert_special(state, view, key);
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return visual_special(state, view, key);
    }
    std::unreachable();
}

// 確定した検索は入力行からだけ来る。INSERT には届かない（斜線は文字・決定 2）。
[[nodiscard]] VimStep pressed(const VimState &state, const VimEditorView &view,
                              const VimSearchPattern &key)
{
    switch (state.mode)
    {
    case VimMode::insert:
        return VimStep{state, VimNoEffect{}};
    case VimMode::normal:
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return searched_key(state, view, key);
    }
    std::unreachable();
}

[[nodiscard]] VimStep stepped(const VimState &state, const VimEditorView &view, VimKey key)
{
    if (state.input_wait.has_value())
    {
        return awaiting_step(state, view, key);
    }
    return std::visit([&](const auto &value) { return pressed(state, view, value); }, key);
}

// ------------------------------------------ 組み立て中の入力を捨てる（Issue #92 / ADR 0032）

// 途中まで組み立てた入力を捨てた状態。engine の外の出来事（入力行の取消・外からの割り込み）の
// あとでは、回数・保留オペレータ・次キー待ち・INSERT の入力記録・組み立て中の `.` の記録は
// どれも続きを持てない。モード・直前の変更・検索と文字検索の記憶・無名レジスタは保つ。
// 捨てる範囲を決めるのはこの 1 か所だけである（ARC-001 / ARC-004）。
[[nodiscard]] VimState input_discarded(const VimState &state)
{
    VimState next = state;
    next.count = std::nullopt;
    next.pending = std::nullopt;
    next.input_wait = std::nullopt;
    next.selected_register = std::nullopt;
    next.insert_repeat = std::nullopt;
    next.recording = std::nullopt;
    next.insert_start = std::nullopt;
    next.block_insert = std::nullopt;
    return next;
}
} // namespace

VimState vim_cancelled_input(const VimState &state)
{
    // 入力行の取消は、割り込みが捨てるものに加えて欲しい列も捨て、モードを休止へ戻す（決定 1）。
    VimState next = input_discarded(state);
    next.wanted_column = std::nullopt;
    next.mode = rested_mode(state.mode);
    return next;
}

VimState vim_interrupted(const VimState &state)
{
    return input_discarded(state);
}

VimState vim_switched_document(const VimState &state, std::optional<VimWantedColumn> wanted_column,
                               std::optional<VimCount> scroll_lines)
{
    // VISUAL と INSERT は文書の上の出来事なので持ち越さない（ADR 0056 の決定 4）。
    VimState next = input_discarded(state);
    next.mode = VimMode::normal;
    next.wanted_column = wanted_column;
    next.scroll_lines = scroll_lines;
    return next;
}

VimState vim_register_stored(const VimState &state, char name, const VimRegister &value)
{
    // 数字と `-` は置き換えるだけ（`:let @0=` に当たる・ADR 0050 の決定 7）。
    const auto numbered = vim_numbered_index(static_cast<char32_t>(name));
    if (numbered.has_value() || name == '-')
    {
        VimState next = state;
        VimRegisterSnapshot &target =
            numbered.has_value() ? next.numbered.registers.at(numbered.value()) : next.small_delete;
        target = VimRegisterSnapshot::from(value);
        return next;
    }
    const auto index = vim_register_index(static_cast<char32_t>(name));
    if (!index.has_value())
    {
        return state;
    }
    VimState next = state;
    VimRegisterSnapshot &target = next.registers.registers.at(index.value());
    target = VimRegisterSnapshot::from(
        name >= 'A' && name <= 'Z' ? appended_recording(target.value(), value.text) : value);
    return next;
}

bool vim_reads_unnamed_clipboard(const VimState &state, const VimKey &key) noexcept
{
    if (!state.clipboard_unnamed || state.selected_register.has_value() ||
        state.pending.has_value() || state.input_wait.has_value() || state.mode != VimMode::normal)
    {
        return false;
    }
    // 鍵 → 動作は表の 1 本（CPP-012）。`p` `P` の文字を比べない。
    const auto *const character = std::get_if<VimCharacter>(&key);
    if (character == nullptr)
    {
        return false;
    }
    const auto action = vim_action_for(character->code);
    return action == VimAction::put_after || action == VimAction::put_before;
}

namespace
{
// 鍵が `+` か `*`（クリップボードの名前）か（#1040）。INSERT の `Ctrl-r`
// は名前がそれのときだけ読む。
[[nodiscard]] bool names_clipboard(const VimKey &key) noexcept
{
    const auto *const character = std::get_if<VimCharacter>(&key);
    if (character == nullptr)
    {
        return false;
    }
    const auto selection = register_selection_of(character->code);
    return selection.has_value() && selection.value().target == VimRegisterTarget::clipboard;
}
} // namespace

bool vim_reads_clipboard(const VimState &state, const VimKey &key) noexcept
{
    if (vim_reads_unnamed_clipboard(state, key))
    {
        return true;
    }
    if (state.selected_register.has_value() &&
        state.selected_register.value().target == VimRegisterTarget::clipboard)
    {
        return true;
    }
    if (!state.input_wait.has_value())
    {
        return false;
    }
    const auto *const prefix = std::get_if<VimPrefix>(&state.input_wait.value());
    if (prefix == nullptr)
    {
        return false;
    }
    return *prefix == VimPrefix::at ||
           (*prefix == VimPrefix::insert_register && names_clipboard(key));
}

VimState vim_clipboard_loaded(const VimState &state, VimRegister value)
{
    VimState next = state;
    next.clipboard = VimRegisterSnapshot::from(value);
    return next;
}

VimState vim_unnamed_loaded(const VimState &state, VimRegister value)
{
    // 自分が書いた本文なら種類（矩形・末尾が改行の文字単位）を保つ（追補（#848）の決定 G）。
    if (value.text == vim_clipboard_text_of(state.unnamed_register.value()))
    {
        return state;
    }
    VimState next = state;
    next.unnamed_register = VimRegisterSnapshot::from(value);
    return next;
}

std::size_t vim_search_count(const VimState &state) noexcept
{
    return resolved_count(state);
}

VimViewport vim_viewport_of(const VimEditorView &view) noexcept
{
    return view.viewport.value_or(VimViewport{LineNumber{1}, 1, 0});
}

VimStep vim_line_jumped(const VimState &state, const VimEditorView &view,
                        std::optional<std::size_t> line)
{
    // 行番号は `{N}G` の回数と同じ値として渡す（回数なしは最後の行・回数 0 は無いので 1 行目）。
    // Vim 9.1 の `:N` は jump ではない（`:help jump-motions` に無い）ので `''` は置かない。
    VimState counted = state;
    counted.pending = std::nullopt;
    counted.count = line.has_value() ? std::optional{VimCount{std::max(line.value(), single_step)}}
                                     : std::nullopt;
    return landed_step(counted, view, VimMotion::document_last);
}

namespace
{
// VISUAL を抜けた鍵が前の選択を置く 1 か所（ADR 0018 の追補・#1039）。Esc・オペレータ・`c` で
// INSERT へ入る道を区別せず、鍵の前が VISUAL で後が VISUAL でなければ鍵の前の選択を覚える
// （Vim の end_visual_mode と do_pending_operator）。`.` の VISUAL の再生は置かず、再生の鍵が
// VISUAL にいるあいだ印を持ち越す（Vim の redo_VIsual_busy・実測）。
[[nodiscard]] VimStep visual_remembered(const VimState &state, const VimEditorView &view,
                                        VimStep step)
{
    if (!is_visual_mode(state.mode))
    {
        return step;
    }
    const bool redoing = state.redoing_visual && view.source == VimKeySource::replayed;
    if (is_visual_mode(step.next.mode))
    {
        step.next.redoing_visual = redoing;
        return step;
    }
    if (redoing)
    {
        return step;
    }
    VimMarks marks = step.marks.value_or(*view.marks);
    marks.last_visual = visual_left(state, view);
    step.marks = std::move(marks);
    return step;
}
} // namespace

VimStep vim_step(const VimState &state, const VimEditorView &view, VimKey key)
{
    auto step = stepped(insert_started(state, view, key), view, key);
    if (step.layout_request.has_value() || step.row_request.has_value() || step.viewport_request)
    {
        return step;
    }
    step = visual_remembered(state, view, std::move(step));
    return macro_recorded(state, view, vim_recorded(state, view, std::move(step), key), key);
}
} // namespace nenenib::core
