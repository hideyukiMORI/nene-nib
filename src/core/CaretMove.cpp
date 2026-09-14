#include "CaretMove.hpp"

#include "Utf8.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 語の区切りは空白だけで見る。記号ごとの区切りは Vim エンジンの縦切りで語の定義と一緒に入れる。
[[nodiscard]] bool space_byte(char value) noexcept
{
    return value == ' ' || value == '\t';
}

[[nodiscard]] std::size_t skip_word(std::string_view content, std::size_t index) noexcept
{
    while (index < content.size() && !space_byte(content[index]))
    {
        ++index;
    }
    return index;
}

[[nodiscard]] std::size_t skip_spaces(std::string_view content, std::size_t index) noexcept
{
    while (index < content.size() && space_byte(content[index]))
    {
        ++index;
    }
    return index;
}

[[nodiscard]] std::size_t skip_word_back(std::string_view content, std::size_t index) noexcept
{
    while (index > 0 && !space_byte(content[index - 1]))
    {
        --index;
    }
    return index;
}

[[nodiscard]] std::size_t skip_spaces_back(std::string_view content, std::size_t index) noexcept
{
    while (index > 0 && space_byte(content[index - 1]))
    {
        --index;
    }
    return index;
}

[[nodiscard]] Offset previous_character(const TextBuffer &text, Offset caret)
{
    const auto position = text.position_of(caret);
    const Offset start = text.line_start(position.line);
    if (caret.value <= start.value)
    {
        return position.line.value > 1 ? text.line_end(LineNumber{position.line.value - 1}) : start;
    }
    const std::string content = text.text_range(start, caret);
    return Offset{start.value + previous_code_point(content, Offset{content.size()}).value};
}

[[nodiscard]] Offset next_character(const TextBuffer &text, Offset caret)
{
    const auto position = text.position_of(caret);
    const Offset end = text.line_end(position.line);
    if (caret.value >= end.value)
    {
        return position.line.value < text.line_count()
                   ? text.line_start(LineNumber{position.line.value + 1})
                   : end;
    }
    const std::string content = text.text_range(caret, end);
    return Offset{caret.value + next_code_point(content, Offset{0}).value};
}

[[nodiscard]] Offset previous_word(const TextBuffer &text, Offset caret)
{
    const auto position = text.position_of(caret);
    const Offset start = text.line_start(position.line);
    if (caret.value <= start.value)
    {
        return previous_character(text, caret);
    }
    const std::string content = text.line_text(position.line);
    const std::size_t index = std::min(caret.value - start.value, content.size());
    return Offset{start.value + skip_word_back(content, skip_spaces_back(content, index))};
}

[[nodiscard]] Offset next_word(const TextBuffer &text, Offset caret)
{
    const auto position = text.position_of(caret);
    const Offset end = text.line_end(position.line);
    if (caret.value >= end.value)
    {
        return next_character(text, caret);
    }
    const std::string content = text.line_text(position.line);
    const std::size_t index = caret.value - text.line_start(position.line).value;
    return Offset{text.line_start(position.line).value +
                  skip_spaces(content, skip_word(content, index))};
}

[[nodiscard]] Offset line_above(const TextBuffer &text, Offset caret, std::size_t count)
{
    const auto position = text.position_of(caret);
    const std::size_t line = position.line.value > count ? position.line.value - count : 1;
    return text.offset_of(TextPosition{LineNumber{line}, position.column});
}

[[nodiscard]] Offset line_below(const TextBuffer &text, Offset caret, std::size_t count)
{
    const auto position = text.position_of(caret);
    const std::size_t line = std::min(position.line.value + count, text.line_count());
    return text.offset_of(TextPosition{LineNumber{line}, position.column});
}
} // namespace

Offset moved_caret(const TextBuffer &text, Offset caret, CaretMotion motion, std::size_t page_lines)
{
    switch (motion)
    {
    case CaretMotion::previous_character:
        return previous_character(text, caret);
    case CaretMotion::next_character:
        return next_character(text, caret);
    case CaretMotion::previous_word:
        return previous_word(text, caret);
    case CaretMotion::next_word:
        return next_word(text, caret);
    case CaretMotion::previous_line:
        return line_above(text, caret, 1);
    case CaretMotion::next_line:
        return line_below(text, caret, 1);
    case CaretMotion::line_start:
        return text.line_start(text.position_of(caret).line);
    case CaretMotion::line_end:
        return text.line_end(text.position_of(caret).line);
    case CaretMotion::page_up:
        return line_above(text, caret, page_lines);
    case CaretMotion::page_down:
        return line_below(text, caret, page_lines);
    case CaretMotion::document_start:
        return Offset{0};
    case CaretMotion::document_end:
        return Offset{text.size_bytes()};
    }
    std::unreachable();
}
} // namespace nenenib::core
