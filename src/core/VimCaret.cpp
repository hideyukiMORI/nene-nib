#include "VimCaret.hpp"

#include "LineNumber.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"

#include <cstddef>
#include <string>

namespace nenenib::core
{
namespace
{
[[nodiscard]] bool blank_byte(char value) noexcept
{
    return value == ' ' || value == '\t';
}
} // namespace

Offset vim_resting_caret(const TextBuffer &text, Offset caret)
{
    const LineNumber line = text.position_of(caret).line;
    const Offset start = text.line_start(line);
    const Offset end = text.line_end(line);
    if (caret.value < end.value)
    {
        return caret;
    }
    if (end.value <= start.value)
    {
        return start;
    }
    const std::string content = text.text_range(start, end);
    return Offset{start.value + previous_code_point(content, Offset{content.size()}).value};
}

Offset vim_first_non_blank(const TextBuffer &text, Offset at)
{
    const LineNumber line = text.position_of(at).line;
    const Offset start = text.line_start(line);
    const std::string content = text.text_range(start, text.line_end(line));
    std::size_t index = 0;
    while (index < content.size() && blank_byte(content[index]))
    {
        ++index;
    }
    if (index >= content.size())
    {
        // 空白しか無い行では Vim の ^ と同じく最後の文字の上に載る（空行なら行頭）。
        return vim_resting_caret(text, text.line_end(line));
    }
    return Offset{start.value + index};
}
} // namespace nenenib::core
