#include "ThemeName.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] bool lower_letter(char letter) noexcept
{
    return letter >= 'a' && letter <= 'z';
}

[[nodiscard]] bool name_character(char letter) noexcept
{
    return lower_letter(letter) || (letter >= '0' && letter <= '9') || letter == '-';
}
} // namespace

ThemeName::ThemeName(std::string name) : name_(std::move(name)) {}

std::expected<ThemeName, ThemeNameFailure> ThemeName::parse(std::string_view name)
{
    if (name.size() > 64)
    {
        return std::unexpected(ThemeNameFailure::too_long);
    }
    if (name.empty() || !lower_letter(name.front()))
    {
        return std::unexpected(ThemeNameFailure::invalid);
    }
    std::string canonical(name);
    for (char &letter : canonical)
    {
        if (letter == '_')
        {
            letter = '-';
        }
        if (!name_character(letter))
        {
            return std::unexpected(ThemeNameFailure::invalid);
        }
    }
    return ThemeName(std::move(canonical));
}

std::string_view ThemeName::text() const noexcept
{
    return name_;
}

bool operator==(const ThemeName &left, const ThemeName &right) noexcept
{
    return left.text() == right.text();
}
} // namespace nenenib::core
