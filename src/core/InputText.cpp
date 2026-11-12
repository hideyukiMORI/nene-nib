#include "InputText.hpp"

#include "Utf8.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
std::expected<InputText, ExFailure> inserted_input(const InputText &input, std::string_view text)
{
    if (!validate_utf8(text) || has_control_character(text))
    {
        return std::unexpected(ExFailure::invalid_text);
    }
    if (text.size() > input_line_hold - std::min(input.text.size(), input_line_hold))
    {
        InputText kept = input;
        kept.lost = true;
        return kept;
    }
    std::string next = input.text;
    next.insert(input.caret.value, text);
    return InputText{std::move(next), Offset{input.caret.value + text.size()}, input.lost};
}

bool overflows(const InputText &input) noexcept
{
    return input.lost || input.text.size() > input_line_limit;
}

OffsetRange input_line_window(std::string_view text, Offset from, Offset to) noexcept
{
    Offset begin{from.value > input_line_window_before ? from.value - input_line_window_before : 0};
    while (begin.value < from.value && !is_boundary(text, begin))
    {
        ++begin.value;
    }
    const std::size_t after = text.size() - std::min(to.value, text.size());
    Offset end{to.value + std::min(after, input_line_window_after)};
    while (end.value > to.value && !is_boundary(text, end))
    {
        --end.value;
    }
    return OffsetRange{begin, end};
}

InputText edited_input(const InputText &input, CommandEdit edit)
{
    InputText next = input;
    const auto before = previous_code_point(input.text, input.caret);
    const auto after = next_code_point(input.text, input.caret);
    switch (edit)
    {
    case CommandEdit::left:
        next.caret = before;
        return next;
    case CommandEdit::right:
        next.caret = after;
        return next;
    case CommandEdit::home:
        next.caret = Offset{0};
        return next;
    case CommandEdit::end:
        next.caret = Offset{input.text.size()};
        return next;
    case CommandEdit::backspace:
        next.text.erase(before.value, input.caret.value - before.value);
        next.caret = before;
        return next;
    case CommandEdit::erase:
        next.text.erase(input.caret.value, after.value - input.caret.value);
        return next;
    case CommandEdit::complete_next:
    case CommandEdit::complete_previous:
    case CommandEdit::page_previous:
    case CommandEdit::page_next:
    case CommandEdit::list_first:
    case CommandEdit::list_last:
    case CommandEdit::history_previous:
    case CommandEdit::history_next:
        // 補完と一覧の送りと履歴は候補・履歴を持つ型だけの動作。文字列とキャレットの規則としては何もしない。
        return next;
    }
    std::unreachable();
}
} // namespace nenenib::core
