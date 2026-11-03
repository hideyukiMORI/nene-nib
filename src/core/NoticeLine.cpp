#include "NoticeLine.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace nenenib::core
{
namespace
{
constexpr unsigned char first_printable = 0x20;
constexpr unsigned char delete_character = 0x7F;
} // namespace

std::optional<DisplayText> notice_line(std::string_view text)
{
    // find_first_of は memchr を core の外へ出す（ARC-003）ので、LspDiagnostics の first_line と
    // 同じく 1 バイトずつ歩く。
    std::size_t line_end = 0;
    while (line_end < text.size() && text[line_end] != '\n' && text[line_end] != '\r')
    {
        ++line_end;
    }
    std::string_view line = text.substr(0, line_end);
    if (line.size() > DisplayText::maximum_bytes)
    {
        std::size_t cut = DisplayText::maximum_bytes;
        while (cut > 0 && !is_boundary(line, Offset{cut}))
        {
            --cut;
        }
        line = line.substr(0, cut);
    }
    std::string shown(line);
    std::ranges::replace_if(
        shown,
        [](char value)
        {
            const auto byte = static_cast<unsigned char>(value);
            return byte < first_printable || byte == delete_character;
        },
        ' ');
    const auto parsed = DisplayText::parse(shown);
    if (!parsed)
    {
        return std::nullopt;
    }
    return parsed.value();
}
} // namespace nenenib::core
