#include "LspPosition.hpp"

#include "LineNumber.hpp"
#include "LspPositionEncoding.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
// 行の内容の先頭から UTF-8 のバイト数 character の位置。コードポイントの途中なら後ろの境目へ。
[[nodiscard]] std::size_t utf8_bytes(std::string_view line, std::size_t character) noexcept
{
    if (character >= line.size())
    {
        return line.size();
    }
    std::size_t at = character;
    while (!is_boundary(line, Offset{at}))
    {
        ++at;
    }
    return at;
}

// 行の内容の先頭から UTF-16 の単位の数 character の位置。代理対の途中なら対の後ろへ。
[[nodiscard]] std::size_t utf16_bytes(std::string_view line, std::size_t character)
{
    std::size_t at = 0;
    std::size_t units = 0;
    while (at < line.size() && units < character)
    {
        const std::size_t next = next_code_point(line, Offset{at}).value;
        units += utf16_length(line.substr(at, next - at)).value_or(1);
        at = next;
    }
    return at;
}

[[nodiscard]] std::size_t bytes_of(std::string_view line, std::size_t character,
                                   LspPositionEncoding encoding)
{
    switch (encoding)
    {
    case LspPositionEncoding::utf8:
        return utf8_bytes(line, character);
    case LspPositionEncoding::utf16:
        return utf16_bytes(line, character);
    }
    return line.size();
}

[[nodiscard]] std::size_t units_of(std::string_view before, LspPositionEncoding encoding)
{
    switch (encoding)
    {
    case LspPositionEncoding::utf8:
        return before.size();
    case LspPositionEncoding::utf16:
        return utf16_length(before).value_or(before.size());
    }
    return before.size();
}
} // namespace

std::optional<Offset> lsp_offset_of(const TextBuffer &buffer, LspPosition position,
                                    LspPositionEncoding encoding)
{
    if (position.line >= buffer.line_count())
    {
        return std::nullopt;
    }
    const LineNumber line{position.line + 1};
    const std::string text = buffer.line_text(line);
    return Offset{buffer.line_start(line).value + bytes_of(text, position.character, encoding)};
}

LspPosition lsp_position_of(const TextBuffer &buffer, Offset at, LspPositionEncoding encoding)
{
    const Offset bounded{std::min(at.value, buffer.size_bytes())};
    const LineNumber line = buffer.position_of(bounded).line;
    const Offset start = buffer.line_start(line);
    const Offset end{std::min(bounded.value, buffer.line_end(line).value)};
    const std::string before = buffer.text_range(start, end);
    return LspPosition{.line = line.value - 1, .character = units_of(before, encoding)};
}
} // namespace nenenib::core