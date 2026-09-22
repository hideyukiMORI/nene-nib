#include "InputText.hpp"

#include "DisplayText.hpp"
#include "Utf8.hpp"

#include <utility>

namespace nenenib::core
{
std::expected<InputText, ExFailure> inserted_input(const InputText &input, std::string_view text)
{
    if (text.size() > DisplayText::maximum_bytes - input.text.size())
    {
        return std::unexpected(ExFailure::too_long);
    }
    if (!validate_utf8(text) || has_control_character(text))
    {
        return std::unexpected(ExFailure::invalid_text);
    }
    std::string next = input.text;
    next.insert(input.caret.value, text);
    return InputText{std::move(next), Offset{input.caret.value + text.size()}};
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
        // 補完は候補を持つ型だけの動作。文字列とキャレットの規則としては何もしない。
        return next;
    }
    std::unreachable();
}
} // namespace nenenib::core
