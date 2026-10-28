#include "VimStepDetail.hpp"

#include "CaretMotion.hpp"
#include "CaretMove.hpp"
#include "Utf8.hpp"
#include "VimAction.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimInsertStart.hpp"
#include "VimScrollDirection.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
using detail::backward_characters;
using detail::display_view;
using detail::finished_input_wait;
using detail::insert_character;
using detail::insert_special;
using detail::last_code_point_size;
using detail::line_feed;
using detail::line_of;
using detail::page_step;
using detail::register_read;
using detail::register_selection_of;
using detail::repeated;
using detail::requested_layout;
using detail::requested_viewport;
using detail::single_step;
using detail::started_prefix;

namespace
{
[[nodiscard]] VimStep insert_moved(const VimState &state, const TextBuffer &text, Offset caret,
                                   CaretMotion motion)
{
    VimState next = state;
    next.insert_repeat = std::nullopt;
    return VimStep{std::move(next),
                   VimMoveTo{moved_caret(
                       text, caret, CaretMoveRequest{motion, single_step, CaretUnit::code_point})}};
}

[[nodiscard]] VimState insert_recorded(const VimState &state, std::string_view input)
{
    VimState next = state;
    if (next.insert_repeat.has_value())
    {
        next.insert_repeat.value().text.append(input);
    }
    return next;
}

[[nodiscard]] VimState insert_record_erased(const VimState &state)
{
    VimState next = state;
    if (!next.insert_repeat.has_value())
    {
        return next;
    }
    std::string &input = next.insert_repeat.value().text;
    if (input.empty())
    {
        next.insert_repeat = std::nullopt;
        return next;
    }
    input.resize(previous_code_point(input, Offset{input.size()}).value);
    return next;
}

// `Ctrl-w` `Ctrl-u` が消した bytes バイトを回数の入力記録から外す（#1040）。記録の全部に届いたら
// 反復をやめる。Vim は記録の鍵を再生し、再生の `Ctrl-w` `Ctrl-u` は前の周の本文まで消すので、
// 本文の記録では写せない（`3Aab<C-u>c` は `xc` だけ・oracle で実測）。
[[nodiscard]] VimState insert_record_cut(const VimState &state, std::size_t bytes)
{
    VimState next = state;
    if (!next.insert_repeat.has_value())
    {
        return next;
    }
    std::string &input = next.insert_repeat.value().text;
    if (bytes >= input.size())
    {
        next.insert_repeat = std::nullopt;
        return next;
    }
    input.resize(input.size() - bytes);
    return next;
}

// 行頭で前の行とつなぐ（`backspace` の eol・#1040）。Backspace も `Ctrl-w` も `Ctrl-u` もここを
// 通り、1 鍵では改行だけを消す。INSERT を始めた行をつないだら、始めの位置はつないだ点へ移る
// （`jI<C-w>x<C-w>` は x だけを消す・oracle で実測）。
[[nodiscard]] VimStep joined_back(const VimState &state, const TextBuffer &text, Offset caret,
                                  LineNumber line)
{
    const LineNumber above{line.value - 1};
    const Offset end = text.line_end(above);
    VimState next = insert_record_erased(state);
    if (next.insert_start.has_value() && next.insert_start.value().line == line)
    {
        next.insert_start = VimInsertStart{above, end.value - text.line_start(above).value};
    }
    return VimStep{std::move(next), VimRemoveRange{OffsetRange{end, caret}}};
}

// INSERT の Backspace。挿入を始めた位置より前も消せる（oracle の backspace=indent,eol,start）。
[[nodiscard]] VimStep insert_erased(const VimState &state, const TextBuffer &text, Offset caret)
{
    if (caret.value == 0)
    {
        return VimStep{state, VimNoEffect{}};
    }
    const LineNumber line = line_of(text, caret);
    if (caret == text.line_start(line))
    {
        return joined_back(state, text, caret, line);
    }
    return VimStep{
        insert_record_erased(state),
        VimRemoveRange{OffsetRange{backward_characters(text, caret, single_step), caret}}};
}

// `Ctrl-w` `Ctrl-u` が戻れる左端（Vim の ins_bs の mincol と Insstart_orig・#1040）。キャレットが
// 始めの行で始めの桁より右にあればそこ、ほかは行頭（autoindent は無いので字下げで止まらない）。
[[nodiscard]] Offset erase_floor(const VimState &state, const TextBuffer &text, Offset caret,
                                 LineNumber line)
{
    const Offset line_start = text.line_start(line);
    if (!state.insert_start.has_value())
    {
        return line_start;
    }
    const VimInsertStart start = state.insert_start.value();
    if (start.line == line && start.byte_column < caret.value - line_start.value)
    {
        return Offset{line_start.value + start.byte_column};
    }
    return line_start;
}

// Vim の vim_isspace（Tab・改行の類・空白）。語の前の空白を先に消すかを決める。
[[nodiscard]] bool erased_blank(char32_t code) noexcept
{
    constexpr char32_t tab = U'\t';
    constexpr char32_t carriage_return = U'\r';
    return code == U' ' || (code >= tab && code <= carriage_return);
}

// INSERT の `Ctrl-w` の左端（#1040）。手前の空白を消してから、同じ種類の文字（`b` と同じ表
// vim_character_class）の連なりを消す。Vim と同じく少なくとも 1 文字は消し、floor で止まる。
[[nodiscard]] std::size_t word_erase_begin(std::string_view content, std::size_t floor)
{
    std::size_t index = content.size();
    std::optional<std::uint32_t> kind;
    do
    {
        const std::size_t previous = vim_character_start(content, Offset{index}).value;
        const char32_t code = code_point_at(content, Offset{previous});
        const std::uint32_t current = vim_character_class(code, VimWordClass::word);
        if (kind.has_value() && (erased_blank(code) || current != kind.value()))
        {
            break;
        }
        if (!kind.has_value() && !erased_blank(code))
        {
            kind = current;
        }
        index = previous;
    } while (index > floor);
    return index;
}

// INSERT の `Ctrl-w` と `Ctrl-u`（#1040）。行頭では前の行とつなぎ、ほかは左端まで（`Ctrl-w` は語の
// 境でも）消す。`.` の記録には鍵のまま残り、再生も同じ消し方をする（Vim の redo と同じ）。
[[nodiscard]] VimStep insert_cut(const VimState &state, const TextBuffer &text, Offset caret,
                                 bool by_word)
{
    if (caret.value == 0)
    {
        return VimStep{state, VimNoEffect{}};
    }
    const LineNumber line = line_of(text, caret);
    const Offset line_start = text.line_start(line);
    if (caret == line_start)
    {
        return joined_back(state, text, caret, line);
    }
    const Offset floor = erase_floor(state, text, caret, line);
    Offset begin = floor;
    if (by_word)
    {
        const std::string content = text.text_range(line_start, caret);
        begin =
            Offset{line_start.value + word_erase_begin(content, floor.value - line_start.value)};
    }
    return VimStep{insert_record_cut(state, caret.value - begin.value),
                   VimRemoveRange{OffsetRange{begin, caret}}};
}

[[nodiscard]] VimStep left_insert(const VimState &state, const VimEditorView &view)
{
    const Offset caret = view.selection.caret;
    const Offset back = backward_characters(view.text, caret, single_step);
    VimStep step{vim_resting_from(state, state.unnamed_register), VimMoveTo{back}};
    // `'^` は 1 つ左へ戻る前のキャレット（Vim の b_last_insert・#1039）。
    step.insert_left = caret.value - back.value;
    if (!state.insert_repeat.has_value() || state.insert_repeat.value().text.empty())
    {
        return step;
    }
    const VimInsertRepeat &repeat = state.insert_repeat.value();
    auto result = repeated(repeat.text, repeat.remaining);
    if (!result.has_value())
    {
        return step;
    }
    std::string body = std::move(result).value();
    const std::size_t retreat = body.back() == '\n' ? 0 : last_code_point_size(body);
    const Offset rest{caret.value + body.size() - retreat};
    step.insert_left = retreat;
    step.effect = VimInsertAt{caret, std::move(body), rest, EditBoundary::absorb};
    return step;
}

[[nodiscard]] VimStep insert_page_moved(const VimState &state, const VimEditorView &view,
                                        VimScrollDirection direction)
{
    const auto action = direction == VimScrollDirection::down ? VimAction::scroll_page_down
                                                              : VimAction::scroll_page_up;
    if (auto request = requested_viewport(state, view, action))
    {
        return std::move(request).value();
    }
    VimState next = state;
    next.insert_repeat = std::nullopt;
    // 格子の画面行の単位は NORMAL と同じ解決を通す（ADR 0144 の決定 5）。論理行の道は今日のまま。
    if (view.layout.enabled && view.layout.unit == VimScrollUnit::row)
    {
        const auto displayed = display_view(next, view, action);
        if (!displayed)
        {
            return requested_layout(state, displayed.error());
        }
        return page_step(next, displayed.value(), direction);
    }
    return page_step(next, view, direction);
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep insert_special(const VimState &state, const VimEditorView &view,
                                     VimSpecialKey key)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    switch (key)
    {
    case VimSpecialKey::escape:
        return left_insert(state, view);
    case VimSpecialKey::enter:
        return VimStep{insert_recorded(state, "\n"), VimNewLine{}};
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
    case VimSpecialKey::control_w:
        return insert_cut(state, text, caret, true);
    case VimSpecialKey::control_u:
        return insert_cut(state, text, caret, false);
    case VimSpecialKey::control_r:
        return started_prefix(state, VimPrefix::insert_register);
    case VimSpecialKey::control_d:
    case VimSpecialKey::control_f:
    case VimSpecialKey::control_b:
    // INSERT の Ctrl+V は窓が OS の貼付のまま扱うのでここへ来ない（ADR 0035 の決定 9）。
    case VimSpecialKey::control_v:
        return VimStep{state, VimNoEffect{}};
    case VimSpecialKey::page_up:
        return insert_page_moved(state, view, VimScrollDirection::up);
    case VimSpecialKey::page_down:
        return insert_page_moved(state, view, VimScrollDirection::down);
    }
    std::unreachable();
}

[[nodiscard]] VimStep insert_character(const VimState &state, VimCharacter key)
{
    if (key.code == line_feed)
    {
        return VimStep{insert_recorded(state, "\n"), VimNewLine{}};
    }
    std::string utf8;
    append_utf8(utf8, key.code);
    return VimStep{insert_recorded(state, utf8), VimInsertString{std::move(utf8)}};
}

// INSERT の `Ctrl-r {name}`（#1040）。名前 → 選択は `"` `@` と同じ register_selection_of、読みは
// register_read の 1 本。本文は 1 回の挿入で入れ（undo は INSERT の 1 単位のまま）、回数の入力記録
// にも足す。名前にならない鍵・空のレジスタは何も入れず INSERT を続ける（Vim の実測）。
[[nodiscard]] VimStep insert_register_step(const VimState &state, const VimEditorView &view,
                                           const VimKey &key)
{
    VimState next = finished_input_wait(state);
    const auto *const character = std::get_if<VimCharacter>(&key);
    const auto selection =
        register_selection_of(character == nullptr ? char32_t{} : character->code);
    if (!selection.has_value())
    {
        return VimStep{std::move(next), VimNoEffect{}};
    }
    std::string body = register_read(state, selection).text;
    if (body.empty())
    {
        return VimStep{std::move(next), VimNoEffect{}};
    }
    const Offset caret = view.selection.caret;
    const Offset rest{caret.value + body.size()};
    next = insert_recorded(next, body);
    return VimStep{std::move(next),
                   VimInsertAt{caret, std::move(body), rest, EditBoundary::absorb}};
}

// INSERT の最初の鍵の前に、始めの位置をキャレットから置く（#1040）。INSERT へ入る命令の効果は
// controller が写すので、入った後の最初の鍵で読むのが Vim の Insstart と同じ位置になる。
[[nodiscard]] VimState insert_started(const VimState &state, const VimEditorView &view)
{
    if (state.mode != VimMode::insert || state.insert_start.has_value())
    {
        return state;
    }
    VimState next = state;
    const Offset caret = view.selection.caret;
    const LineNumber line = line_of(view.text, caret);
    next.insert_start = VimInsertStart{line, caret.value - view.text.line_start(line).value};
    return next;
}
} // namespace detail
} // namespace nenenib::core
