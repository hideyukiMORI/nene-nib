#include "ThemeChoice.hpp"

#include "BuiltinThemes.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] Theme view_of(BuiltinTheme value) noexcept
{
    return theme_of(value);
}

[[nodiscard]] Theme view_of(const std::shared_ptr<const ThemeDocument> &value) noexcept
{
    return theme_view(*value);
}
} // namespace

ThemeChoice::ThemeChoice(Value value) : value_(std::move(value)) {}

ThemeChoice ThemeChoice::from(BuiltinTheme theme)
{
    return ThemeChoice(theme);
}

ThemeChoice ThemeChoice::from(ThemeDocument theme)
{
    return ThemeChoice(std::make_shared<const ThemeDocument>(std::move(theme)));
}

Theme ThemeChoice::view() const & noexcept
{
    return std::visit([](const auto &value) { return view_of(value); }, value_);
}

std::string_view ThemeChoice::name() const & noexcept
{
    return view().name;
}

bool operator==(const ThemeChoice &left, const ThemeChoice &right) noexcept
{
    return left.name() == right.name();
}
} // namespace nenenib::core
