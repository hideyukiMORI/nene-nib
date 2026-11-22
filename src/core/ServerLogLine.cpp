#include "ServerLogLine.hpp"

#include "Utf8.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
constexpr std::string_view replacement = "\xEF\xBF\xBD";
constexpr std::string_view ellipsis = "\xE2\x80\xA6";
constexpr unsigned char first_printable = 0x20;
constexpr unsigned char delete_character = 0x7F;
constexpr unsigned char tab_character = 0x09;

// 先頭のバイトが始める列の長さ（1〜4）。列を始めないバイト（継続・0xC0 0xC1・0xF5 以上）は 0。
[[nodiscard]] std::size_t sequence_length(unsigned char lead) noexcept
{
    constexpr unsigned char ascii_end = 0x80;
    constexpr unsigned char two_first = 0xC2;
    constexpr unsigned char three_first = 0xE0;
    constexpr unsigned char four_first = 0xF0;
    constexpr unsigned char four_last = 0xF4;
    if (lead < ascii_end)
    {
        return 1;
    }
    if (lead < two_first)
    {
        return 0;
    }
    if (lead < three_first)
    {
        return 2;
    }
    if (lead < four_first)
    {
        return 3;
    }
    return lead <= four_last ? 4 : 0;
}

// at から始まる 1 つの code point の長さ。正しい列でなければ 0（呼び手が 1 バイトを置き換える）。
[[nodiscard]] std::size_t valid_sequence(std::string_view raw, std::size_t at) noexcept
{
    const std::size_t length = sequence_length(static_cast<unsigned char>(raw[at]));
    if (length == 0 || raw.size() - at < length)
    {
        return 0;
    }
    return validate_utf8(raw.substr(at, length)).has_value() ? length : 0;
}

[[nodiscard]] bool control(unsigned char byte) noexcept
{
    return (byte < first_printable && byte != tab_character) || byte == delete_character;
}

// 置き換えた行（上限で切る前）。
[[nodiscard]] std::string repaired(std::string_view raw)
{
    std::string line;
    line.reserve(raw.size());
    std::size_t at = 0;
    while (at < raw.size())
    {
        const std::size_t length = valid_sequence(raw, at);
        if (length == 0 || (length == 1 && control(static_cast<unsigned char>(raw[at]))))
        {
            line.append(replacement);
            ++at;
            continue;
        }
        line.append(raw.substr(at, length));
        at += length;
    }
    return line;
}
} // namespace

std::string server_log_line(std::string_view raw)
{
    std::string line = repaired(raw);
    if (line.size() <= server_log_line_bytes)
    {
        return line;
    }
    const std::size_t kept = server_log_line_bytes - ellipsis.size();
    line.resize(whole_code_points(std::string_view(line).substr(0, kept)));
    line.append(ellipsis);
    return line;
}
} // namespace nenenib::core
