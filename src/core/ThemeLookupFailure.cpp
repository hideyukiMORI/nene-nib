#include "ThemeLookupFailure.hpp"

#include <utility>

namespace nenenib::core
{
bool operator==(const ThemeLookupFailure &left, const ThemeLookupFailure &right) noexcept
{
    return left.name == right.name && left.reason == right.reason;
}

std::string_view theme_failure_reason(ThemeFailure failure) noexcept
{
    switch (failure)
    {
    case ThemeFailure::not_found:
        return "Theme file not found";
    case ThemeFailure::unreadable:
        return "Theme file could not be read";
    case ThemeFailure::too_large:
        return "Theme file exceeds 16 KiB";
    case ThemeFailure::malformed:
        return "Expected key=value lines";
    case ThemeFailure::duplicate_key:
        return "Duplicate theme field";
    case ThemeFailure::unknown_key:
        return "Unknown theme field";
    case ThemeFailure::missing_field:
        return "Required theme field missing";
    case ThemeFailure::unsupported_version:
        return "Unsupported theme version";
    case ThemeFailure::invalid_name:
        return "Invalid theme filename or name";
    case ThemeFailure::name_mismatch:
        return "Theme name does not match filename";
    case ThemeFailure::reserved_name:
        return "Reserved theme name";
    case ThemeFailure::invalid_text:
        return "Invalid UTF-8 or theme attribution";
    case ThemeFailure::invalid_appearance:
        return "Appearance must be dark or light";
    case ThemeFailure::invalid_color:
        return "Invalid RGB or RGBA color";
    case ThemeFailure::insufficient_contrast:
        return "Text/background contrast must be at least 4.5";
    }
    std::unreachable();
}

DisplayText theme_failure_message(const ThemeLookupFailure &failure)
{
    return DisplayText::parse(std::string(failure.name.text()) + ": " +
                              std::string(theme_failure_reason(failure.reason)))
        .value();
}
} // namespace nenenib::core
