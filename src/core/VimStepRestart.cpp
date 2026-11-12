// INSERT の `Ctrl-o`（ADR 0182）。印を立てる `Ctrl-o`・命令が終わったかを決める vim_step の出口の
// 1 か所・始め直す vim_restarted_insert の 3 つをこの翻訳単位に置く（ADR 0158 の決定 3 の群）。
#include "VimStepDetail.hpp"

#include "VimColumnWish.hpp"
#include "VimInsertRestart.hpp"
#include "VimInsertRestartPhase.hpp"
#include "VimInsertStart.hpp"
#include "VimKeySource.hpp"
#include "VimOperator.hpp"
#include "VimReplayKind.hpp"
#include "VimSpecialKey.hpp"
#include "VirtualColumn.hpp"

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace nenenib::core
{
using detail::backward_characters;
using detail::counts_as_digit;
using detail::entered_insert;
using detail::forward_characters;
using detail::line_of;
using detail::single_step;

namespace
{
[[nodiscard]] bool is_control_o(const VimKey &key) noexcept
{
    const auto *const special = std::get_if<VimSpecialKey>(&key);
    return special != nullptr && *special == VimSpecialKey::control_o;
}

[[nodiscard]] bool is_character(const VimKey &key, char32_t first, char32_t second) noexcept
{
    const auto *const character = std::get_if<VimCharacter>(&key);
    return character != nullptr && (character->code == first || character->code == second);
}

// 命令が 1 つ終わったか（Vim の normal_cmd の終わりの条件・決定 2）。オペレータ待ち・回数・
// `"x`・1 鍵の引数の待ち・VISUAL・入力行を開いた鍵の間は戻らない（入力行は評価の後に controller
// が vim_restarted_insert を呼ぶ・決定 8）。
[[nodiscard]] bool command_finished(const VimState &next, const VimEffect &effect) noexcept
{
    return next.mode == VimMode::normal && !next.input_wait.has_value() &&
           !next.pending.has_value() && !next.count.has_value() &&
           !next.selected_register.has_value() &&
           !std::holds_alternative<VimOpenCommandLine>(effect) &&
           !std::holds_alternative<VimOpenSearch>(effect);
}

// Vim の OP_CHANGE で INSERT へ入る鍵か（`c` の演算・NORMAL の `s` `C`・VISUAL の `c` `s` `C`
// `S` `R`・ops.c の case OP_CHANGE）。
[[nodiscard]] bool enters_change(const VimState &before, const VimKey &key) noexcept
{
    if (before.pending.has_value() && before.pending.value().operation == VimOperator::change)
    {
        return true;
    }
    if (before.input_wait.has_value())
    {
        return false;
    }
    if (is_visual_mode(before.mode))
    {
        return is_character(key, U'c', U's') || is_character(key, U'C', U'S') ||
               is_character(key, U'R', U'R');
    }
    return is_character(key, U's', U'C');
}

// VISUAL の `I` `A`（Vim の op_insert は印をいつも保つ・ops.c の case OP_INSERT）。
[[nodiscard]] bool enters_block_insert(const VimState &before, const VimKey &key) noexcept
{
    return is_visual_mode(before.mode) && !before.input_wait.has_value() &&
           is_character(key, U'I', U'A');
}

// INSERT へ入る命令の後も印を保つか（決定 4）。OP_CHANGE は打った鍵でなければ保ち（Vim の
// KeyTyped）、`i a A o O gi` は印を立てた後の再生（`<C-o>.`・Vim の invoke_edit の !stuff_empty()）
// でだけ保つ。印を立てた `Ctrl-o` が再生の中の鍵なら、同じ再生の中の `i` は印を消す（マクロの中の
// `<C-o>i`・typeahead の鍵は stuff ではない）。
[[nodiscard]] bool keeps_restart(const VimState &before, const VimEditorView &view,
                                 const VimKey &key, const VimInsertRestart &mark) noexcept
{
    if (enters_block_insert(before, key))
    {
        return true;
    }
    if (enters_change(before, key))
    {
        return view.source != VimKeySource::typed;
    }
    return vim_key_replayed(view.source) && !mark.replayed;
}

// `0` `^` `<Home>`（Vim の nv_beginline と nv_home は ins_at_eol を下ろす・1 文字の行で行末の後ろへ
// 進めないため）。回数の続きの `0` と 1 鍵の引数は除く。
[[nodiscard]] bool goes_to_line_start(const VimState &before, const VimKey &key) noexcept
{
    if (before.mode == VimMode::insert || before.input_wait.has_value())
    {
        return false;
    }
    if (const auto *const special = std::get_if<VimSpecialKey>(&key))
    {
        return *special == VimSpecialKey::home;
    }
    return is_character(key, U'^', U'^') ||
           (is_character(key, U'0', U'0') && !counts_as_digit(before, U'0'));
}

constexpr char line_feed_byte = 0x0A;

// 印を待つ間の命令のうち Vim がカーソルを変える 2 つ（決定 2 の位置の規則が読む）。`D` は `d$`
// なので curswant が行末（MAXCOL）、1 行の中の文字単位の `p` `P` は貼った本文の後ろ（register.c の
// do_put の「CTRL-O p」・restart_edit が立っている間）。
[[nodiscard]] VimStep paused_command_adjusted(const VimState &before, const TextBuffer &text,
                                              VimStep step, const VimKey &key)
{
    if (before.mode != VimMode::normal || before.input_wait.has_value() ||
        before.pending.has_value())
    {
        return step;
    }
    if (is_character(key, U'D', U'D'))
    {
        step.next.wanted_column = VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
        return step;
    }
    auto *const put = std::get_if<VimInsertAt>(&step.effect);
    if (put != nullptr && is_character(key, U'p', U'P') && !put->utf8.empty() &&
        put->utf8.find(line_feed_byte) == std::string::npos)
    {
        put->caret = Offset{put->at.value + put->utf8.size()};
        // 行の内容の終わりへ貼ったら Vim のカーソルは行末の後ろのまま（NORMAL の寄せが最後の文字へ
        // 戻しても、始め直す位置の規則が後ろへ進める）。
        if (put->at == text.line_end(line_of(text, put->at)))
        {
            step.next.wanted_column = VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
        }
    }
    return step;
}

// 欲しい列が今の仮想桁より右か（Vim の curswant > w_virtcol・`^O$` `^O80|`・短い行への `j` `k`）。
[[nodiscard]] bool wants_beyond(const VimState &state, const TextBuffer &text, Offset caret)
{
    if (!state.wanted_column.has_value())
    {
        return false;
    }
    const VimWantedColumn &wanted = state.wanted_column.value();
    switch (wanted.wish)
    {
    case VimColumnWish::at_line_end:
        return true;
    case VimColumnWish::at_column:
        return wanted.column.value > caret_virtual_column(text, caret).value;
    }
    std::unreachable();
}

// 始め直す位置（edit.c の 355〜375 行）。`Ctrl-o` の前に行の内容の終わりにいて同じ行にまだいるか、
// 欲しい列が今の桁より右で、キャレットが行の最後の文字の上なら、その文字の後ろへ進める。
[[nodiscard]] Offset restarted_caret(const VimState &state, const VimEditorView &view,
                                     const VimInsertRestart &mark)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    const LineNumber line = line_of(text, caret);
    const Offset end = text.line_end(line);
    if (caret.value >= end.value)
    {
        return caret;
    }
    const bool stayed = mark.at_line_end && line == mark.line;
    if (!stayed && !wants_beyond(state, text, caret))
    {
        return caret;
    }
    return forward_characters(text, caret, single_step).value >= end.value ? end : caret;
}
} // namespace

namespace detail
{
bool pauses_insert(const VimState &state, const VimKey &key) noexcept
{
    return state.mode == VimMode::insert && !state.input_wait.has_value() && is_control_o(key);
}

// 始め直した INSERT でまだ何も打っていないか、矢印などで区切った後にまだ打っていないか（Vim の
// arrow_used）。
bool restarted_untyped(const VimState &state) noexcept
{
    const bool arrowed = state.insert_start.has_value() && state.insert_start.value().moved;
    return arrowed || (state.insert_restart.has_value() &&
                       state.insert_restart.value().phase == VimInsertRestartPhase::restarted);
}

// INSERT の `Ctrl-o`（Vim の ins_ctrl_o と ins_esc・決定 3）。Esc と同じ道で INSERT を抜け（`'^`
// を置き・回数の反復は捨て・`.` の記録の確定は vim_recorded が Esc と同じに行う）、左へ戻すのは
// 行の内容の終わりにいるときだけ。印は待ちの段で立てる。
VimStep insert_paused(const VimState &state, const VimEditorView &view)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    const LineNumber line = line_of(text, caret);
    const bool at_end = caret.value >= text.line_end(line).value;
    const Offset back = at_end && caret.value > text.line_start(line).value
                            ? backward_characters(text, caret, single_step)
                            : caret;
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.insert_restart =
        VimInsertRestart{line, at_end, restarted_untyped(state), vim_key_replayed(view.source),
                         VimInsertRestartPhase::waiting};
    // 左へ戻しても欲しい列は戻す前の桁のまま（Vim の ins_esc は restart_edit があると
    // w_set_curswant を立てない・`GkiX<C-o>kY` は上の行の 2 桁目へ）。
    // 始め直した後に何も打っていなければ、Vim の curswant は始め直す前のまま（`<C-o>j<C-o>j`）。
    if (restarted_untyped(state) && state.wanted_column.has_value())
    {
        next.wanted_column = state.wanted_column;
    }
    else if (!(back == caret))
    {
        next.wanted_column =
            VimWantedColumn{VimColumnWish::at_column, caret_virtual_column(text, caret)};
    }
    VimStep step{std::move(next), VimMoveTo{back}};
    step.insert_left = caret.value - back.value;
    return step;
}

// 印の段を進める出口の 1 か所（決定 2）。命令が終わった step は due にし、controller が効果を写した
// 後に vim_restarted_insert で始め直す（始め直す位置の規則は効果を写した後のキャレットを読む）。
// INSERT へ入る命令は keeps_restart で印を消すかを決め、`<C-o>@a` は Vim と同じに印を消して再生の
// 後ろに `i` を積む（決定 7）。
VimStep insert_restart_noted(const VimState &before, const VimEditorView &view, VimStep step,
                             const VimKey &key)
{
    if (!step.next.insert_restart.has_value() || pauses_insert(before, key))
    {
        return step;
    }
    VimInsertRestart &mark = step.next.insert_restart.value();
    if (mark.phase == VimInsertRestartPhase::restarted)
    {
        return step;
    }
    if (before.mode != VimMode::insert && step.next.mode == VimMode::insert &&
        !keeps_restart(before, view, key, mark))
    {
        step.next.insert_restart = std::nullopt;
        return step;
    }
    if (auto *const replay = std::get_if<VimReplay>(&step.effect);
        replay != nullptr && replay->kind == VimReplayKind::macro)
    {
        step.next.insert_restart = std::nullopt;
        replay->insert_after = true;
        return step;
    }
    mark.at_line_end = mark.at_line_end && !goes_to_line_start(before, key);
    step = paused_command_adjusted(before, view.text, std::move(step), key);
    mark.phase = command_finished(step.next, step.effect) ? VimInsertRestartPhase::due
                                                          : VimInsertRestartPhase::waiting;
    return step;
}

// `.` が繰り返す変更（決定 6・Vim の nv_dot の `restart_edit != 0 && !arrow_used`）。印を待つ
// 間で、`Ctrl-o` の前の INSERT で何か打っていたなら、INSERT へ入る前の変更を繰り返す。
const std::optional<VimRepeatRecord> &repeated_record(const VimState &state) noexcept
{
    const auto &mark = state.insert_restart;
    if (mark.has_value() && mark.value().phase != VimInsertRestartPhase::restarted &&
        !mark.value().arrow_used)
    {
        return state.change_before_insert;
    }
    return state.last_change;
}
} // namespace detail

bool vim_insert_paused(const VimState &state) noexcept
{
    return state.insert_restart.has_value() &&
           state.insert_restart.value().phase != VimInsertRestartPhase::restarted;
}

bool vim_insert_restart_due(const VimState &state) noexcept
{
    return state.insert_restart.has_value() &&
           state.insert_restart.value().phase == VimInsertRestartPhase::due;
}

VimStep vim_restarted_insert(const VimState &state, const VimEditorView &view)
{
    if (!state.insert_restart.has_value())
    {
        return VimStep{state, VimNoEffect{}};
    }
    VimInsertRestart mark = state.insert_restart.value();
    const Offset caret = restarted_caret(state, view, mark);
    // `i` と同じ入口（回数は使わない・Vim の edit(restart_edit, FALSE, 1)）。
    VimState resting = state;
    resting.count = std::nullopt;
    VimStep step = entered_insert(resting, caret);
    step.next.last_change = state.last_change;
    // 始め直しは curswant を変えない（何も打たずに次の `<C-o>` の `j` `k` が同じ列を使う）。
    step.next.wanted_column = state.wanted_column;
    const LineNumber line = line_of(view.text, caret);
    // 始めの位置は始め直した所で、矢印の後と同じ印を立てる（Vim の Insstart と arrow_used = TRUE・
    // edit.c の 228〜238 行と 350 行）。最初に打った鍵が `.` の記録を `1i` から取り直す。
    step.next.insert_start =
        VimInsertStart{line, caret.value - view.text.line_start(line).value, true};
    mark.phase = VimInsertRestartPhase::restarted;
    step.next.insert_restart = mark;
    return step;
}
} // namespace nenenib::core
