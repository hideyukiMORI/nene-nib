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
#include "VimWordMotion.hpp"
#include "VimWordStop.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
constexpr std::size_t single_step = 1;
constexpr std::size_t decimal_base = 10;
constexpr char32_t line_feed = U'\n';
// Ctrl-r は文字としては制御文字なので、表に載せる値は名前で書く（原文に制御文字を置かない）。
constexpr char32_t control_r_character = 0x12;

// NORMAL の鍵 → 動作の表（ADR 0012 の決定 5 / CPP-012）。分岐で書くと関数長で落ちる（T8）。
// 数字は表に無い。回数として積むほうが先で、'0' だけは回数が空のときに行頭として引かれる。
constexpr std::array<VimBinding, 16> normal_bindings{{{U'h', VimAction::move_left},
                                                      {U'j', VimAction::move_down},
                                                      {U'k', VimAction::move_up},
                                                      {U'l', VimAction::move_right},
                                                      {U'0', VimAction::move_line_start},
                                                      {U'$', VimAction::move_line_end},
                                                      {U'w', VimAction::move_next_word},
                                                      {U'b', VimAction::move_previous_word},
                                                      {U'x', VimAction::remove_character},
                                                      {U'd', VimAction::remove_operator},
                                                      {U'i', VimAction::insert_before},
                                                      {U'a', VimAction::insert_after},
                                                      {U'I', VimAction::insert_at_line_start},
                                                      {U'A', VimAction::insert_at_line_end},
                                                      {U'u', VimAction::undo},
                                                      {control_r_character, VimAction::redo}}};

// オペレータの後ろで範囲になる動作。ここに無い鍵（x i a …）は保留中の d を打ち消す。
constexpr std::array<VimMotionBinding, 8> motion_bindings{
    {{VimAction::move_left, VimMotion::left},
     {VimAction::move_down, VimMotion::down},
     {VimAction::move_up, VimMotion::up},
     {VimAction::move_right, VimMotion::right},
     {VimAction::move_line_start, VimMotion::line_start},
     {VimAction::move_line_end, VimMotion::line_end},
     {VimAction::move_next_word, VimMotion::next_word},
     {VimAction::move_previous_word, VimMotion::previous_word}}};

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

[[nodiscard]] std::size_t count_of(const VimState &state) noexcept
{
    return state.count.has_value() ? state.count.value().value : single_step;
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
    const std::size_t count = count_of(state);
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
    case VimMotion::line_end:
        return vim_resting_caret(text, text.line_end(line));
    case VimMotion::next_word:
        return vim_resting_caret(text,
                                 vim_next_word(text, caret, count, VimWordStop::across_lines));
    case VimMotion::previous_word:
        return vim_previous_word(text, caret, count);
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
    case VimMotion::next_word:
    case VimMotion::previous_word:
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

// 文字単位の削除。範囲が空なら Vim と同じく何も起きず、無名レジスタも書き換わらない。
[[nodiscard]] VimStep remove_characters(const VimState &state, const TextBuffer &text,
                                        const OffsetRange &range)
{
    if (is_empty(range))
    {
        return cancelled(state);
    }
    return VimStep{vim_resting_state(text.text_range(range.begin, range.end)),
                   VimRemoveRange{range}};
}

// 行単位の削除（dd dj dk）。無名レジスタには行の内容と改行が入る（最終行にも Vim は付ける）。
// 最終行を消すときだけ、消す範囲は 1 つ前の行の末尾から始まる（改行を 1 つだけ消すため）。
[[nodiscard]] VimStep remove_lines(const TextBuffer &text, LineNumber first, LineNumber last)
{
    const Offset content_begin = text.line_start(first);
    const Offset content_end = text.line_terminator_end(last);
    std::string removed = text.text_range(content_begin, content_end);
    const bool through_end = last.value >= text.line_count();
    if (through_end)
    {
        removed.push_back('\n');
    }
    const Offset begin =
        through_end && first.value > 1 ? text.line_end(LineNumber{first.value - 1}) : content_begin;
    return VimStep{vim_resting_state(std::move(removed)),
                   VimRemoveLines{OffsetRange{begin, content_end}}};
}

[[nodiscard]] VimStep removed_by_motion(const VimState &state, const TextBuffer &text, Offset caret,
                                        VimMotion motion)
{
    const std::size_t count = count_of(state);
    const LineNumber line = line_of(text, caret);
    switch (motion)
    {
    case VimMotion::down:
        return line.value + count > text.line_count()
                   ? cancelled(state)
                   : remove_lines(text, line, LineNumber{line.value + count});
    case VimMotion::up:
        return line.value <= count ? cancelled(state)
                                   : remove_lines(text, LineNumber{line.value - count}, line);
    case VimMotion::left:
        return remove_characters(state, text,
                                 OffsetRange{backward_characters(text, caret, count), caret});
    case VimMotion::right:
        return remove_characters(state, text,
                                 OffsetRange{caret, forward_characters(text, caret, count)});
    case VimMotion::line_start:
        return remove_characters(state, text, OffsetRange{text.line_start(line), caret});
    case VimMotion::line_end:
        return remove_characters(state, text, OffsetRange{caret, text.line_end(line)});
    case VimMotion::next_word:
        return remove_characters(
            state, text,
            OffsetRange{caret, vim_next_word(text, caret, count, VimWordStop::at_line_end)});
    case VimMotion::previous_word:
        return remove_characters(state, text,
                                 OffsetRange{vim_previous_word(text, caret, count), caret});
    }
    std::unreachable();
}

[[nodiscard]] VimStep entered_insert(const VimState &state, Offset caret)
{
    VimState next = vim_resting_state(state.unnamed_register);
    next.mode = VimMode::insert;
    return VimStep{std::move(next), VimMoveTo{caret}};
}

[[nodiscard]] VimStep pending_remove(const VimState &state)
{
    VimState next = vim_resting_state(state.unnamed_register);
    next.count = state.count;
    next.pending = VimOperator::remove;
    return VimStep{std::move(next), VimNoEffect{}};
}

[[nodiscard]] VimStep commanded(const VimState &state, const TextBuffer &text, Offset caret,
                                VimAction action)
{
    const LineNumber line = line_of(text, caret);
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
    case VimAction::remove_character:
        return remove_characters(
            state, text, OffsetRange{caret, forward_characters(text, caret, count_of(state))});
    case VimAction::remove_operator:
        return pending_remove(state);
    case VimAction::insert_before:
        return entered_insert(state, caret);
    case VimAction::insert_after:
        return entered_insert(state, forward_characters(text, caret, single_step));
    case VimAction::insert_at_line_start:
        return entered_insert(state, vim_first_non_blank(text, caret));
    case VimAction::insert_at_line_end:
        return entered_insert(state, text.line_end(line));
    case VimAction::undo:
        return VimStep{vim_resting_state(state.unnamed_register), VimUndo{}};
    case VimAction::redo:
        return VimStep{vim_resting_state(state.unnamed_register), VimRedo{}};
    }
    std::unreachable();
}

// 保留中の d の後ろ。もう一度 d なら行単位、移動なら範囲、ほかの鍵なら打ち消し（Vim と同じ）。
[[nodiscard]] VimStep pending_step(const VimState &state, const TextBuffer &text, Offset caret,
                                   VimAction action)
{
    if (action == VimAction::remove_operator)
    {
        const LineNumber line = line_of(text, caret);
        const std::size_t last = line.value + count_of(state) - single_step;
        return last > text.line_count() ? cancelled(state)
                                        : remove_lines(text, line, LineNumber{last});
    }
    const auto motion = motion_for(action);
    if (!motion.has_value())
    {
        return cancelled(state);
    }
    return removed_by_motion(state, text, caret, motion.value());
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
    if (state.pending.has_value())
    {
        return pending_step(state, text, caret, action.value());
    }
    return commanded(state, text, caret, action.value());
}

// NORMAL の特別な鍵。矢印は h j k l と同じ扱いで、Esc は保留を捨てるだけ（ADR 0012 の決定 5）。
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
        return normal_character(state, text, caret, VimCharacter{U'h'});
    case VimSpecialKey::arrow_right:
        return normal_character(state, text, caret, VimCharacter{U'l'});
    case VimSpecialKey::arrow_up:
        return normal_character(state, text, caret, VimCharacter{U'k'});
    case VimSpecialKey::arrow_down:
        return normal_character(state, text, caret, VimCharacter{U'j'});
    case VimSpecialKey::control_r:
        return normal_character(state, text, caret, VimCharacter{control_r_character});
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
