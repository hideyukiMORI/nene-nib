#include "VimStepDetail.hpp"

#include "CaretMotion.hpp"
#include "CaretMove.hpp"
#include "Utf8.hpp"
#include "VimAction.hpp"
#include "VimScrollDirection.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
using detail::backward_characters;
using detail::display_view;
using detail::insert_character;
using detail::insert_special;
using detail::last_code_point_size;
using detail::line_feed;
using detail::line_of;
using detail::page_step;
using detail::repeated;
using detail::requested_layout;
using detail::requested_viewport;
using detail::single_step;

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
    return VimStep{insert_record_erased(state), VimRemoveRange{OffsetRange{begin, caret}}};
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
    case VimSpecialKey::control_r:
    case VimSpecialKey::control_d:
    case VimSpecialKey::control_u:
    case VimSpecialKey::control_f:
    case VimSpecialKey::control_b:
    // INSERT の Ctrl+V は窓が OS の貼付のまま扱うのでここへ来ない（ADR 0035 の決定 9）。
    case VimSpecialKey::control_v:
    // INSERT の Ctrl-w（Vim では語の削除）は未対応で、失敗にせず何もしない（ADR 0148 の決定 7）。
    case VimSpecialKey::control_w:
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
} // namespace detail
} // namespace nenenib::core
