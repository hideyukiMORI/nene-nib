#include "UriScheme.hpp"

#include <cstddef>
#include <optional>
#include <string_view>

namespace nenenib::core
{
namespace
{
constexpr std::size_t shortest_scheme = 2;

[[nodiscard]] bool ascii_letter(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

[[nodiscard]] bool ascii_digit(char value) noexcept
{
    return value >= '0' && value <= '9';
}

// scheme の 2 文字目から後ろに来てよい文字（英数字と `+` `-` `.`）。
[[nodiscard]] bool scheme_character(char value) noexcept
{
    return ascii_letter(value) || ascii_digit(value) || value == '+' || value == '-' ||
           value == '.';
}
} // namespace

std::optional<std::size_t> uri_scheme_length(std::string_view text) noexcept
{
    if (text.empty() || !ascii_letter(text[0]))
    {
        return std::nullopt;
    }
    std::size_t index = 1;
    while (index < text.size() && scheme_character(text[index]))
    {
        ++index;
    }
    if (index < shortest_scheme || index == text.size() || text[index] != ':')
    {
        return std::nullopt;
    }
    return index;
}
} // namespace nenenib::core
