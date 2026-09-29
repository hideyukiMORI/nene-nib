#include "VimCaret.hpp"

#include "LineNumber.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
[[nodiscard]] bool blank_byte(char value) noexcept
{
    return value == ' ' || value == '\t';
}

// column のバイトを含む Vim の文字の先頭（ADR 0053 の決定 5）。code point の途中なら code point
// の先頭へ、結合文字の上ならそれが付く文字の先頭へ。column は content の内側。
[[nodiscard]] Offset character_containing(std::string_view content, std::size_t column) noexcept
{
    const Offset code_point = is_boundary(content, Offset{column})
                                  ? Offset{column}
                                  : previous_code_point(content, Offset{column});
    return vim_character_start(content, vim_character_end(content, code_point));
}
} // namespace

Offset vim_resting_caret(const TextBuffer &text, Offset caret)
{
    const LineNumber line = text.position_of(caret).line;
    const Offset start = text.line_start(line);
    const Offset end = text.line_end(line);
    if (end.value <= start.value)
    {
        return start;
    }
    // 文字の途中（結合文字の上）には立たない。行末を越えたら行の最後の文字の先頭（ADR 0053 の
    // 決定 5）。
    const std::string content = text.text_range(start, end);
    if (caret.value < end.value)
    {
        return Offset{start.value + character_containing(content, caret.value - start.value).value};
    }
    return Offset{start.value + vim_character_start(content, Offset{content.size()}).value};
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

Offset vim_line_and_column(const TextBuffer &text, LineNumber line, std::size_t column)
{
    const Offset start = text.line_start(line);
    const std::string content = text.text_range(start, text.line_end(line));
    if (column >= content.size())
    {
        return Offset{start.value + content.size()};
    }
    return Offset{start.value + character_containing(content, column).value};
}

Offset vim_same_line_and_column(const TextBuffer &before, Offset at, const TextBuffer &after)
{
    const LineNumber line = before.position_of(at).line;
    const std::size_t column = at.value - before.line_start(line).value;
    if (line.value > after.line_count())
    {
        return vim_first_non_blank(after, after.line_start(LineNumber{after.line_count()}));
    }
    return vim_line_and_column(after, line, column);
}
} // namespace nenenib::core
