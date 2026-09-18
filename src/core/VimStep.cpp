#include "VimStep.hpp"

#include "CaretMotion.hpp"
#include "CaretMove.hpp"
#include "Column.hpp"
#include "LineNumber.hpp"
#include "OffsetRange.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimBinding.hpp"
#include "VimCaret.hpp"
#include "VimMotionBinding.hpp"
#include "VimMotionRange.hpp"
#include "VimPutSide.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimWordEndStop.hpp"
#include "VimWordMotion.hpp"
#include "VimWordStop.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
constexpr std::size_t single_step = 1;
constexpr std::size_t decimal_base = 10;
constexpr char32_t line_feed = U'\n';
constexpr char carriage_return = '\r';
// Ctrl-r は文字としては制御文字なので、表に載せる値は名前で書く（原文に制御文字を置かない）。
constexpr char32_t control_r_character = 0x12;

// NORMAL の鍵 → 動作の表（ADR 0012 の決定 5 / ADR 0015 の決定 6 / CPP-012）。分岐で書くと
// 関数長で落ちる（T8）。数字は表に無い。回数として積むほうが先で、'0' だけは回数が空のときに
// 行頭として引かれる。
constexpr std::array<VimBinding, 25> normal_bindings{{{U'h', VimAction::move_left},
                                                      {U'j', VimAction::move_down},
                                                      {U'k', VimAction::move_up},
                                                      {U'l', VimAction::move_right},
                                                      {U'0', VimAction::move_line_start},
                                                      {U'$', VimAction::move_line_end},
                                                      {U'w', VimAction::move_next_word},
                                                      {U'b', VimAction::move_previous_word},
                                                      {U'e', VimAction::move_word_end},
                                                      {U'^', VimAction::move_first_non_blank},
                                                      {U'x', VimAction::remove_character},
                                                      {U'd', VimAction::remove_operator},
                                                      {U'c', VimAction::change_operator},
                                                      {U'y', VimAction::yank_operator},
                                                      {U'p', VimAction::put_after},
                                                      {U'P', VimAction::put_before},
                                                      {U'D', VimAction::remove_to_line_end},
                                                      {U'C', VimAction::change_to_line_end},
                                                      {U'Y', VimAction::yank_line},
                                                      {U'i', VimAction::insert_before},
                                                      {U'a', VimAction::insert_after},
                                                      {U'I', VimAction::insert_at_line_start},
                                                      {U'A', VimAction::insert_at_line_end},
                                                      {U'u', VimAction::undo},
                                                      {control_r_character, VimAction::redo}}};

// オペレータの後ろで範囲になる動作。ここに無い鍵（x i a …）は保留中のオペレータを打ち消す。
constexpr std::array<VimMotionBinding, 10> motion_bindings{
    {{VimAction::move_left, VimMotion::left},
     {VimAction::move_down, VimMotion::down},
     {VimAction::move_up, VimMotion::up},
     {VimAction::move_right, VimMotion::right},
     {VimAction::move_line_start, VimMotion::line_start},
     {VimAction::move_line_end, VimMotion::line_end},
     {VimAction::move_next_word, VimMotion::next_word},
     {VimAction::move_previous_word, VimMotion::previous_word},
     {VimAction::move_word_end, VimMotion::word_end},
     {VimAction::move_first_non_blank, VimMotion::first_non_blank}}};

// 終わりの位置を範囲に入れない移動（Vim の exclusive）。$ と e は入れる（inclusive）。
// 行単位の j k はどちらでもないので、この表には無い。
constexpr std::array<VimMotion, 6> exclusive_motions{
    {VimMotion::left, VimMotion::right, VimMotion::line_start, VimMotion::first_non_blank,
     VimMotion::next_word, VimMotion::previous_word}};

[[nodiscard]] std::optional<VimAction> action_for(char32_t key) noexcept
{
    for (const VimBinding binding : normal_bindings)
    {
        if (binding.key == key)
        {
            return binding.action;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<VimMotion> motion_for(VimAction action) noexcept
{
    for (const VimMotionBinding binding : motion_bindings)
    {
        if (binding.action == action)
        {
            return binding.motion;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool exclusive(VimMotion motion) noexcept
{
    for (const VimMotion entry : exclusive_motions)
    {
        if (entry == motion)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::size_t count_of(const std::optional<VimCount> &count) noexcept
{
    return count.has_value() ? count.value().value : single_step;
}

// 動作の回数。オペレータ側の回数と、そのあとに積んだ回数の掛け算（ADR 0015 の決定 1）。
[[nodiscard]] std::size_t total_count(const VimState &state) noexcept
{
    const std::size_t pending =
        state.pending.has_value() ? count_of(state.pending.value().count) : single_step;
    return pending * count_of(state.count);
}

[[nodiscard]] LineNumber line_of(const TextBuffer &text, Offset caret)
{
    return text.position_of(caret).line;
}

// 行の中で count 文字ぶん先。行の内容の終わり（Vim が NUL を置く桁）までは進める。
[[nodiscard]] Offset forward_characters(const TextBuffer &text, Offset caret, std::size_t count)
{
    const LineNumber line = line_of(text, caret);
    const Offset start = text.line_start(line);
    const std::string content = text.text_range(start, text.line_end(line));
    std::size_t index = caret.value - start.value;
    for (std::size_t step = 0; step < count && index < content.size(); ++step)
    {
        index = next_code_point(content, Offset{index}).value;
    }
    return Offset{start.value + index};
}

[[nodiscard]] Offset backward_characters(const TextBuffer &text, Offset caret, std::size_t count)
{
    const LineNumber line = line_of(text, caret);
    const Offset start = text.line_start(line);
    const std::string content = text.text_range(start, text.line_end(line));
    std::size_t index = caret.value - start.value;
    for (std::size_t step = 0; step < count && index > 0; ++step)
    {
        index = previous_code_point(content, Offset{index}).value;
    }
    return Offset{start.value + index};
}

// キャレットの下に空白でない文字があるか（Vim の gchar_cursor() != NUL && !VIM_ISWHITE）。
// 行の内容の終わり（NUL の桁）と空行は「文字が無い」ので偽。cw の特例が効く条件そのもの。
[[nodiscard]] bool word_under_caret(const TextBuffer &text, Offset caret)
{
    const LineNumber line = line_of(text, caret);
    if (caret.value >= text.line_end(line).value)
    {
        return false;
    }
    const std::string under = text.text_range(caret, forward_characters(text, caret, single_step));
    return under != " " && under != "\t";
}

[[nodiscard]] VimWantedColumn wanted_column_of(const TextBuffer &text, const VimState &state,
                                               Offset caret)
{
    if (state.wanted_column.has_value())
    {
        return state.wanted_column.value();
    }
    return VimWantedColumn{VimColumnWish::at_column, text.position_of(caret).column};
}

// 欲しい列（curswant）で別の行へ。$ が貼り付けた「行末」はその行の最後の文字になる。
[[nodiscard]] Offset caret_on_line(const TextBuffer &text, const VimWantedColumn &wanted,
                                   LineNumber line)
{
    switch (wanted.wish)
    {
    case VimColumnWish::at_line_end:
        return vim_resting_caret(text, text.line_end(line));
    case VimColumnWish::at_column:
        return vim_resting_caret(text, text.offset_of(TextPosition{line, wanted.column}));
    }
    std::unreachable();
}

[[nodiscard]] LineNumber line_below(const TextBuffer &text, LineNumber line, std::size_t count)
{
    const std::size_t wanted = line.value + count;
    return LineNumber{wanted > text.line_count() ? text.line_count() : wanted};
}

[[nodiscard]] LineNumber line_above(LineNumber line, std::size_t count)
{
    return LineNumber{line.value > count ? line.value - count : 1};
}

[[nodiscard]] Offset moved_by(const TextBuffer &text, const VimState &state, Offset caret,
                              VimMotion motion)
{
    const std::size_t count = count_of(state.count);
    const LineNumber line = line_of(text, caret);
    switch (motion)
    {
    case VimMotion::left:
        return backward_characters(text, caret, count);
    case VimMotion::right:
        return vim_resting_caret(text, forward_characters(text, caret, count));
    case VimMotion::up:
        return caret_on_line(text, wanted_column_of(text, state, caret), line_above(line, count));
    case VimMotion::down:
        return caret_on_line(text, wanted_column_of(text, state, caret),
                             line_below(text, line, count));
    case VimMotion::line_start:
        return text.line_start(line);
    case VimMotion::first_non_blank:
        return vim_first_non_blank(text, caret);
    case VimMotion::line_end:
        return vim_resting_caret(text, text.line_end(line_below(text, line, count - single_step)));
    case VimMotion::next_word:
        return vim_resting_caret(text,
                                 vim_next_word(text, caret, count, VimWordStop::across_lines));
    case VimMotion::previous_word:
        return vim_previous_word(text, caret, count);
    case VimMotion::word_end:
    case VimMotion::word_end_for_change:
        return vim_resting_caret(
            text, vim_word_end(text, caret, count, VimWordEndStop::enter_the_next_word));
    }
    std::unreachable();
}

// 移動のあとの欲しい列。j / k は前の値を引き継ぎ、$ は「行末」を貼り、ほかは動いた先の桁。
[[nodiscard]] VimWantedColumn wanted_after(const TextBuffer &text, const VimWantedColumn &wanted,
                                           Offset moved, VimMotion motion)
{
    switch (motion)
    {
    case VimMotion::up:
    case VimMotion::down:
        return wanted;
    case VimMotion::line_end:
        return VimWantedColumn{VimColumnWish::at_line_end, Column{1}};
    case VimMotion::left:
    case VimMotion::right:
    case VimMotion::line_start:
    case VimMotion::first_non_blank:
    case VimMotion::next_word:
    case VimMotion::previous_word:
    case VimMotion::word_end:
    case VimMotion::word_end_for_change:
        return VimWantedColumn{VimColumnWish::at_column, text.position_of(moved).column};
    }
    std::unreachable();
}

[[nodiscard]] VimStep cancelled(const VimState &state)
{
    return VimStep{vim_resting_state(state.unnamed_register), VimNoEffect{}};
}

[[nodiscard]] VimStep motion_step(const VimState &state, const TextBuffer &text, Offset caret,
                                  VimMotion motion)
{
    const VimWantedColumn wanted = wanted_column_of(text, state, caret);
    const Offset moved = moved_by(text, state, caret, motion);
    VimState next = vim_resting_state(state.unnamed_register);
    next.wanted_column = wanted_after(text, wanted, moved, motion);
    return VimStep{std::move(next), VimMoveTo{moved}};
}

// ---------------------------------------------------------------- 範囲（ADR 0015 の決定 2）

// 文字単位の範囲。^ は前にも後ろにも動くので、端は必ず小さいほうを先にする（Vim も同じ）。
[[nodiscard]] VimMotionRange characters_between(Offset first, Offset second)
{
    const bool ordered = first.value <= second.value;
    return VimMotionRange{OffsetRange{ordered ? first : second, ordered ? second : first},
                          VimRegisterKind::characters};
}

// 行単位の範囲。最初の行の先頭から、最後の行の内容の終わりまで（改行は入れない）。
[[nodiscard]] VimMotionRange lines_between(const TextBuffer &text, LineNumber first,
                                           LineNumber last)
{
    return VimMotionRange{OffsetRange{text.line_start(first), text.line_end(last)},
                          VimRegisterKind::lines};
}

// 下へ steps 行ぶんの行の範囲。Vim の cursor_down は最終行にいるときだけ失敗し、
// ほかは本文の末尾で止まる（5dd が 3 行の本文を全部消し、j2dd が最終行では何もしない理由）。
[[nodiscard]] std::optional<VimMotionRange> lines_below(const TextBuffer &text, LineNumber line,
                                                        std::size_t steps)
{
    if (steps > 0 && line.value >= text.line_count())
    {
        return std::nullopt;
    }
    return lines_between(text, line, line_below(text, line, steps));
}

[[nodiscard]] std::optional<VimMotionRange> lines_above(const TextBuffer &text, LineNumber line,
                                                        std::size_t steps)
{
    if (steps > 0 && line.value <= 1)
    {
        return std::nullopt;
    }
    return lines_between(text, line_above(line, steps), line);
}

// $ は inclusive で、回数があると count-1 行下の行末まで（Vim の nv_dollar の cursor_down）。
[[nodiscard]] std::optional<VimMotionRange> to_line_end(const TextBuffer &text, Offset caret,
                                                        std::size_t count)
{
    const LineNumber line = line_of(text, caret);
    if (count > single_step && line.value >= text.line_count())
    {
        return std::nullopt;
    }
    return characters_between(caret, text.line_end(line_below(text, line, count - single_step)));
}

// inclusive な移動の範囲の終わり。止まった文字を範囲に入れる（行の内容の終わりは越えない）。
[[nodiscard]] Offset inclusive_end(const TextBuffer &text, Offset at)
{
    return forward_characters(text, at, single_step);
}

[[nodiscard]] std::optional<VimMotionRange> motion_range(const TextBuffer &text, Offset caret,
                                                         VimMotion motion, std::size_t count)
{
    const LineNumber line = line_of(text, caret);
    switch (motion)
    {
    case VimMotion::down:
        return lines_below(text, line, count);
    case VimMotion::up:
        return lines_above(text, line, count);
    case VimMotion::left:
        return characters_between(backward_characters(text, caret, count), caret);
    case VimMotion::right:
        return characters_between(caret, forward_characters(text, caret, count));
    case VimMotion::line_start:
        return characters_between(text.line_start(line), caret);
    case VimMotion::first_non_blank:
        return characters_between(vim_first_non_blank(text, caret), caret);
    case VimMotion::line_end:
        return to_line_end(text, caret, count);
    case VimMotion::next_word:
        return characters_between(caret,
                                  vim_next_word(text, caret, count, VimWordStop::at_line_end));
    case VimMotion::previous_word:
        return characters_between(vim_previous_word(text, caret, count), caret);
    case VimMotion::word_end:
        return characters_between(
            caret, inclusive_end(text, vim_word_end(text, caret, count,
                                                    VimWordEndStop::enter_the_next_word)));
    case VimMotion::word_end_for_change:
        return characters_between(
            caret, inclusive_end(
                       text, vim_word_end(text, caret, count, VimWordEndStop::stay_in_this_word)));
    }
    std::unreachable();
}

// 始まりが行の字下げの中か（Vim の inindent）。空行の行頭も、最初の非空白そのものも中に数える。
[[nodiscard]] bool starts_in_the_indent(const TextBuffer &text, Offset begin)
{
    return begin.value <= vim_first_non_blank(text, begin).value;
}

// at から行の内容の終わりまでが空白だけか（Vim の skipwhite が NUL に着くか）。
[[nodiscard]] bool blank_to_the_line_end(const TextBuffer &text, Offset at)
{
    const std::string rest = text.text_range(at, text.line_end(line_of(text, at)));
    // 走査は自前で書く。STL の検索は core の外へ memchr を出す（ARC-003 の許可シンボル）。
    for (const char byte : rest)
    {
        if (byte != ' ' && byte != '\t')
        {
            return false;
        }
    }
    return true;
}

// :help exclusive の 2 つの言い換え。行頭で終わる exclusive な移動は 1 つ前の行の末尾までになり、
// 始まりが字下げの中なら行単位になる。dw が空行を丸ごと消すのも、db が上の行を消すのもこれ。
[[nodiscard]] VimMotionRange adjusted_for_exclusive(const TextBuffer &text,
                                                    const VimMotionRange &range, VimMotion motion)
{
    const LineNumber first = line_of(text, range.range.begin);
    const LineNumber last = line_of(text, range.range.end);
    if (!exclusive(motion) || first.value >= last.value || range.range.end != text.line_start(last))
    {
        return range;
    }
    const LineNumber previous{last.value - 1};
    if (starts_in_the_indent(text, range.range.begin))
    {
        return lines_between(text, first, previous);
    }
    return characters_between(range.range.begin, text.line_end(previous));
}

// op_delete の「奇妙な Vi の振る舞い」。複数行にまたがる文字単位の削除で、終わりの後ろが空白だけ
// かつ始まりが字下げの中なら行単位になる（2D と de の行またぎ）。d だけで、c と y には無い。
[[nodiscard]] VimMotionRange whole_lines_for_delete(const TextBuffer &text,
                                                    const VimMotionRange &range)
{
    const LineNumber first = line_of(text, range.range.begin);
    const LineNumber last = line_of(text, range.range.end);
    if (range.kind == VimRegisterKind::lines || first.value >= last.value ||
        !blank_to_the_line_end(text, range.range.end) ||
        !starts_in_the_indent(text, range.range.begin))
    {
        return range;
    }
    return lines_between(text, first, last);
}

// ---------------------------------------------------------------- オペレータ（決定 2・3）

// レジスタの本文は LF だけ（決定 3）。CRLF の文書から取った本文はここで CR を落とす。
[[nodiscard]] std::string without_carriage_returns(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (const char byte : text)
    {
        if (byte != carriage_return)
        {
            result.push_back(byte);
        }
    }
    return result;
}

[[nodiscard]] VimRegister register_of(const TextBuffer &text, const VimMotionRange &range)
{
    std::string body =
        without_carriage_returns(text.text_range(range.range.begin, range.range.end));
    switch (range.kind)
    {
    case VimRegisterKind::lines:
        // 行単位のレジスタは必ず改行で終わる（Vim は最終行にも付ける）。
        body.push_back('\n');
        return VimRegister{std::move(body), VimRegisterKind::lines};
    case VimRegisterKind::characters:
        return VimRegister{std::move(body), VimRegisterKind::characters};
    }
    std::unreachable();
}

// 空の文字単位の範囲は無名レジスタを書き換えない（空行の D が実測でそう振る舞う）。
[[nodiscard]] VimRegister register_after(const VimState &state, const TextBuffer &text,
                                         const VimMotionRange &range)
{
    if (range.kind == VimRegisterKind::characters && is_empty(range.range))
    {
        return state.unnamed_register;
    }
    return register_of(text, range);
}

// 行を消す範囲。最終行を消すときだけ 1 つ前の行の末尾から始める（改行を 1 つだけ消すため）。
[[nodiscard]] OffsetRange removed_lines_range(const TextBuffer &text, const VimMotionRange &range)
{
    const LineNumber first = line_of(text, range.range.begin);
    const LineNumber last = line_of(text, range.range.end);
    const bool through_end = last.value >= text.line_count();
    const Offset begin = through_end && first.value > 1 ? text.line_end(LineNumber{first.value - 1})
                                                        : range.range.begin;
    return OffsetRange{begin, text.line_terminator_end(last)};
}

[[nodiscard]] VimStep removed(const VimState &state, const TextBuffer &text,
                              const VimMotionRange &range)
{
    const VimMotionRange whole = whole_lines_for_delete(text, range);
    VimState next = vim_resting_state(register_after(state, text, whole));
    switch (whole.kind)
    {
    case VimRegisterKind::characters:
        return VimStep{std::move(next), VimRemoveRange{whole.range}};
    case VimRegisterKind::lines:
        return VimStep{std::move(next), VimRemoveLines{removed_lines_range(text, whole)}};
    }
    std::unreachable();
}

// c。効果は削除そのままで、次の状態が INSERT（決定 2）。行単位でも改行は残す＝行が 1 本残る。
[[nodiscard]] VimStep changed(const VimState &state, const TextBuffer &text,
                              const VimMotionRange &range)
{
    VimState next = vim_resting_state(register_after(state, text, range));
    next.mode = VimMode::insert;
    return VimStep{std::move(next), VimRemoveRange{range.range}};
}

// y のあとのキャレット。文字単位は範囲の先頭、行単位は範囲の最初の行の同じ桁（実測）。
[[nodiscard]] Offset yanked_caret(const TextBuffer &text, const VimState &state, Offset caret,
                                  const VimMotionRange &range)
{
    switch (range.kind)
    {
    case VimRegisterKind::characters:
        return range.range.begin;
    case VimRegisterKind::lines:
        return caret_on_line(text, wanted_column_of(text, state, caret),
                             line_of(text, range.range.begin));
    }
    std::unreachable();
}

[[nodiscard]] VimStep yanked(const VimState &state, const TextBuffer &text, Offset caret,
                             const VimMotionRange &range)
{
    return VimStep{vim_resting_state(register_after(state, text, range)),
                   VimMoveTo{yanked_caret(text, state, caret, range)}};
}

// いま効かせるオペレータ。保留が無ければ「消す」＝ x が通る道（x は保留を立てずにここへ来る）。
[[nodiscard]] VimOperator operation_of(const VimState &state) noexcept
{
    return state.pending.has_value() ? state.pending.value().operation : VimOperator::remove;
}

[[nodiscard]] VimStep performed(const VimState &state, const TextBuffer &text, Offset caret,
                                const VimMotionRange &range)
{
    switch (operation_of(state))
    {
    case VimOperator::remove:
        return removed(state, text, range);
    case VimOperator::change:
        return changed(state, text, range);
    case VimOperator::yank:
        return yanked(state, text, caret, range);
    }
    std::unreachable();
}

// cw の特例（Vim の nv_wordcmd）。語の上（非空白）では ce の範囲になり、空白の上は dw と同じ。
[[nodiscard]] VimMotion motion_for_change(const TextBuffer &text, Offset caret,
                                          VimOperator operation, VimMotion motion)
{
    if (operation != VimOperator::change || motion != VimMotion::next_word ||
        !word_under_caret(text, caret))
    {
        return motion;
    }
    return VimMotion::word_end_for_change;
}

[[nodiscard]] VimStep operated(const VimState &state, const TextBuffer &text, Offset caret,
                               VimMotion motion)
{
    const VimMotion wanted = motion_for_change(text, caret, operation_of(state), motion);
    const auto range = motion_range(text, caret, wanted, total_count(state));
    if (!range.has_value())
    {
        return cancelled(state);
    }
    return performed(state, text, caret, adjusted_for_exclusive(text, range.value(), wanted));
}

// オペレータを自分の回数といっしょに保留する（決定 1）。積んだ回数はここで移る。
[[nodiscard]] VimStep pending_operator(const VimState &state, VimOperator operation)
{
    VimState next = vim_resting_state(state.unnamed_register);
    next.pending = VimPendingOperator{operation, state.count};
    return VimStep{std::move(next), VimNoEffect{}};
}

// dd cc yy と Y。自分の行から count-1 行下まで。保留を立ててから 1 本の performed に流す。
[[nodiscard]] VimStep operated_on_lines(const VimState &state, const TextBuffer &text, Offset caret,
                                        VimOperator operation)
{
    const auto range = lines_below(text, line_of(text, caret), total_count(state) - single_step);
    if (!range.has_value())
    {
        return cancelled(state);
    }
    VimState with = state;
    with.pending = VimPendingOperator{operation, std::nullopt};
    return performed(with, text, caret, range.value());
}

// D と C。d$ / c$ と同じ経路（決定 6）。回数は $ が受け取る（2D が 2 行ぶんになる）。
[[nodiscard]] VimStep operated_to_line_end(const VimState &state, const TextBuffer &text,
                                           Offset caret, VimOperator operation)
{
    VimState with = state;
    with.pending = VimPendingOperator{operation, std::nullopt};
    return operated(with, text, caret, VimMotion::line_end);
}

// ---------------------------------------------------------------- p / P（決定 4）

[[nodiscard]] std::string repeated(std::string_view body, std::size_t count)
{
    std::string result;
    result.reserve(body.size() * count);
    for (std::size_t step = 0; step < count; ++step)
    {
        result.append(body);
    }
    return result;
}

// 貼った本文の最後の code point の長さ。文字単位の put はその文字の上にキャレットが載る。
[[nodiscard]] std::size_t last_code_point_size(std::string_view body)
{
    return body.size() - previous_code_point(body, Offset{body.size()}).value;
}

// 貼った行の最初の非空白までのバイト数。行単位の put はそこへキャレットが載る。
[[nodiscard]] std::size_t first_non_blank_index(std::string_view body)
{
    std::size_t index = 0;
    while (index < body.size() && (body[index] == ' ' || body[index] == '\t'))
    {
        ++index;
    }
    return index;
}

[[nodiscard]] VimPutString put_characters(const TextBuffer &text, Offset caret, std::string body,
                                          VimPutSide side)
{
    const Offset at =
        side == VimPutSide::after ? forward_characters(text, caret, single_step) : caret;
    const Offset rest{at.value + body.size() - last_code_point_size(body)};
    return VimPutString{at, std::move(body), rest};
}

// 行単位の put。上の行へ入れるときと最終行でない行の下へ入れるときは行の先頭にそのまま入る。
// 最終行の下だけは、そこにもう改行が無いので本文の末尾に改行から書く。
[[nodiscard]] VimPutString put_lines(const TextBuffer &text, Offset caret, std::string body,
                                     VimPutSide side)
{
    const LineNumber line = line_of(text, caret);
    const std::size_t indent = first_non_blank_index(body);
    if (side == VimPutSide::before)
    {
        const Offset at = text.line_start(line);
        return VimPutString{at, std::move(body), Offset{at.value + indent}};
    }
    if (line.value < text.line_count())
    {
        const Offset at = text.line_terminator_end(line);
        return VimPutString{at, std::move(body), Offset{at.value + indent}};
    }
    const Offset at = text.line_end(line);
    std::string tail = "\n";
    tail.append(body, 0, body.size() - single_step);
    return VimPutString{at, std::move(tail), Offset{at.value + single_step + indent}};
}

[[nodiscard]] VimStep put_step(const VimState &state, const TextBuffer &text, Offset caret,
                               VimPutSide side)
{
    if (state.unnamed_register.text.empty())
    {
        return cancelled(state);
    }
    std::string body = repeated(state.unnamed_register.text, count_of(state.count));
    VimState next = vim_resting_state(state.unnamed_register);
    switch (state.unnamed_register.kind)
    {
    case VimRegisterKind::characters:
        return VimStep{std::move(next), put_characters(text, caret, std::move(body), side)};
    case VimRegisterKind::lines:
        return VimStep{std::move(next), put_lines(text, caret, std::move(body), side)};
    }
    std::unreachable();
}

// ---------------------------------------------------------------- 鍵から動作へ

[[nodiscard]] VimStep entered_insert(const VimState &state, Offset caret)
{
    VimState next = vim_resting_state(state.unnamed_register);
    next.mode = VimMode::insert;
    return VimStep{std::move(next), VimMoveTo{caret}};
}

// x。保留中のオペレータの無い削除なので、範囲を作って同じ 1 本（performed）に流す。
[[nodiscard]] VimStep removed_character(const VimState &state, const TextBuffer &text, Offset caret)
{
    return performed(
        state, text, caret,
        characters_between(caret, forward_characters(text, caret, count_of(state.count))));
}

[[nodiscard]] VimStep commanded(const VimState &state, const TextBuffer &text, Offset caret,
                                VimAction action)
{
    switch (action)
    {
    case VimAction::move_left:
        return motion_step(state, text, caret, VimMotion::left);
    case VimAction::move_down:
        return motion_step(state, text, caret, VimMotion::down);
    case VimAction::move_up:
        return motion_step(state, text, caret, VimMotion::up);
    case VimAction::move_right:
        return motion_step(state, text, caret, VimMotion::right);
    case VimAction::move_line_start:
        return motion_step(state, text, caret, VimMotion::line_start);
    case VimAction::move_line_end:
        return motion_step(state, text, caret, VimMotion::line_end);
    case VimAction::move_next_word:
        return motion_step(state, text, caret, VimMotion::next_word);
    case VimAction::move_previous_word:
        return motion_step(state, text, caret, VimMotion::previous_word);
    case VimAction::move_word_end:
        return motion_step(state, text, caret, VimMotion::word_end);
    case VimAction::move_first_non_blank:
        return motion_step(state, text, caret, VimMotion::first_non_blank);
    case VimAction::remove_character:
        return removed_character(state, text, caret);
    case VimAction::remove_operator:
        return pending_operator(state, VimOperator::remove);
    case VimAction::change_operator:
        return pending_operator(state, VimOperator::change);
    case VimAction::yank_operator:
        return pending_operator(state, VimOperator::yank);
    case VimAction::put_after:
        return put_step(state, text, caret, VimPutSide::after);
    case VimAction::put_before:
        return put_step(state, text, caret, VimPutSide::before);
    case VimAction::remove_to_line_end:
        return operated_to_line_end(state, text, caret, VimOperator::remove);
    case VimAction::change_to_line_end:
        return operated_to_line_end(state, text, caret, VimOperator::change);
    case VimAction::yank_line:
        return operated_on_lines(state, text, caret, VimOperator::yank);
    case VimAction::insert_before:
        return entered_insert(state, caret);
    case VimAction::insert_after:
        return entered_insert(state, forward_characters(text, caret, single_step));
    case VimAction::insert_at_line_start:
        return entered_insert(state, vim_first_non_blank(text, caret));
    case VimAction::insert_at_line_end:
        return entered_insert(state, text.line_end(line_of(text, caret)));
    case VimAction::undo:
        return VimStep{vim_resting_state(state.unnamed_register), VimUndo{}};
    case VimAction::redo:
        return VimStep{vim_resting_state(state.unnamed_register), VimRedo{}};
    }
    std::unreachable();
}

// 同じオペレータの鍵をもう一度押したときの動作（dd cc yy）。ほかの鍵は打ち消しになる。
[[nodiscard]] VimAction doubled_action_of(VimOperator operation) noexcept
{
    switch (operation)
    {
    case VimOperator::remove:
        return VimAction::remove_operator;
    case VimOperator::change:
        return VimAction::change_operator;
    case VimOperator::yank:
        return VimAction::yank_operator;
    }
    std::unreachable();
}

// 保留中のオペレータの後ろ。同じ鍵なら行単位、移動なら範囲、ほかの鍵なら打ち消し（Vim と同じ）。
[[nodiscard]] VimStep pending_step(const VimState &state, const TextBuffer &text, Offset caret,
                                   VimAction action)
{
    const VimOperator operation = operation_of(state);
    if (action == doubled_action_of(operation))
    {
        return operated_on_lines(state, text, caret, operation);
    }
    const auto motion = motion_for(action);
    if (!motion.has_value())
    {
        return cancelled(state);
    }
    return operated(state, text, caret, motion.value());
}

[[nodiscard]] VimStep acted(const VimState &state, const TextBuffer &text, Offset caret,
                            VimAction action)
{
    if (state.pending.has_value())
    {
        return pending_step(state, text, caret, action);
    }
    return commanded(state, text, caret, action);
}

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
    const std::size_t carried =
        state.count.has_value() ? state.count.value().value * decimal_base : 0;
    VimState next = state;
    next.count = VimCount{carried + digit};
    return VimStep{std::move(next), VimNoEffect{}};
}

[[nodiscard]] VimStep normal_character(const VimState &state, const TextBuffer &text, Offset caret,
                                       VimCharacter key)
{
    if (counts_as_digit(state, key.code))
    {
        return counted(state, key.code);
    }
    const auto action = action_for(key.code);
    if (!action.has_value())
    {
        return cancelled(state);
    }
    return acted(state, text, caret, action.value());
}

// NORMAL の特別な鍵。矢印は h j k l と同じ動作で、Home / End は 0 と $（ADR 0015 の決定 6）。
// Esc は保留を捨てるだけ（ADR 0012 の決定 5）。
[[nodiscard]] VimStep normal_special(const VimState &state, const TextBuffer &text, Offset caret,
                                     VimSpecialKey key)
{
    switch (key)
    {
    case VimSpecialKey::escape:
    case VimSpecialKey::enter:
    case VimSpecialKey::backspace:
        return cancelled(state);
    case VimSpecialKey::arrow_left:
        return acted(state, text, caret, VimAction::move_left);
    case VimSpecialKey::arrow_right:
        return acted(state, text, caret, VimAction::move_right);
    case VimSpecialKey::arrow_up:
        return acted(state, text, caret, VimAction::move_up);
    case VimSpecialKey::arrow_down:
        return acted(state, text, caret, VimAction::move_down);
    case VimSpecialKey::home:
        return acted(state, text, caret, VimAction::move_line_start);
    case VimSpecialKey::end:
        return acted(state, text, caret, VimAction::move_line_end);
    case VimSpecialKey::control_r:
        return acted(state, text, caret, VimAction::redo);
    }
    std::unreachable();
}

[[nodiscard]] VimStep insert_moved(const VimState &state, const TextBuffer &text, Offset caret,
                                   CaretMotion motion)
{
    return VimStep{state, VimMoveTo{moved_caret(text, caret, motion, single_step)}};
}

// INSERT の Backspace。挿入を始めた位置より前も消せる（oracle の backspace=indent,eol,start）。
[[nodiscard]] VimStep insert_erased(const VimState &state, const TextBuffer &text, Offset caret)
{
    if (caret.value == 0)
    {
        return VimStep{state, VimNoEffect{}};
    }
    const LineNumber line = line_of(text, caret);
    const Offset start = text.line_start(line);
    const Offset begin = caret.value > start.value ? backward_characters(text, caret, single_step)
                                                   : text.line_end(LineNumber{line.value - 1});
    return VimStep{state, VimRemoveRange{OffsetRange{begin, caret}}};
}

[[nodiscard]] VimStep insert_special(const VimState &state, const TextBuffer &text, Offset caret,
                                     VimSpecialKey key)
{
    switch (key)
    {
    case VimSpecialKey::escape:
        return VimStep{vim_resting_state(state.unnamed_register),
                       VimMoveTo{backward_characters(text, caret, single_step)}};
    case VimSpecialKey::enter:
        return VimStep{state, VimNewLine{}};
    case VimSpecialKey::backspace:
        return insert_erased(state, text, caret);
    case VimSpecialKey::arrow_left:
        return insert_moved(state, text, caret, CaretMotion::previous_character);
    case VimSpecialKey::arrow_right:
        return insert_moved(state, text, caret, CaretMotion::next_character);
    case VimSpecialKey::arrow_up:
        return insert_moved(state, text, caret, CaretMotion::previous_line);
    case VimSpecialKey::arrow_down:
        return insert_moved(state, text, caret, CaretMotion::next_line);
    case VimSpecialKey::home:
        return insert_moved(state, text, caret, CaretMotion::line_start);
    case VimSpecialKey::end:
        return insert_moved(state, text, caret, CaretMotion::line_end);
    case VimSpecialKey::control_r:
        return VimStep{state, VimNoEffect{}};
    }
    std::unreachable();
}

[[nodiscard]] VimStep insert_character(const VimState &state, VimCharacter key)
{
    if (key.code == line_feed)
    {
        return VimStep{state, VimNewLine{}};
    }
    std::string utf8;
    append_utf8(utf8, key.code);
    return VimStep{state, VimInsertString{std::move(utf8)}};
}

[[nodiscard]] VimStep normal_step(const VimState &state, const TextBuffer &text, Offset caret,
                                  VimKey key)
{
    if (std::holds_alternative<VimCharacter>(key))
    {
        return normal_character(state, text, caret, std::get<VimCharacter>(key));
    }
    return normal_special(state, text, caret, std::get<VimSpecialKey>(key));
}

[[nodiscard]] VimStep insert_step(const VimState &state, const TextBuffer &text, Offset caret,
                                  VimKey key)
{
    if (std::holds_alternative<VimCharacter>(key))
    {
        return insert_character(state, std::get<VimCharacter>(key));
    }
    return insert_special(state, text, caret, std::get<VimSpecialKey>(key));
}
} // namespace

VimStep vim_step(const VimState &state, const TextBuffer &text, Offset caret, VimKey key)
{
    switch (state.mode)
    {
    case VimMode::normal:
        return normal_step(state, text, caret, key);
    case VimMode::insert:
        return insert_step(state, text, caret, key);
    }
    std::unreachable();
}
} // namespace nenenib::core
