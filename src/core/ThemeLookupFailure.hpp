#pragma once

#include "DisplayText.hpp"
#include "ThemeFailure.hpp"
#include "ThemeName.hpp"

namespace nenenib::core
{
struct ThemeLookupFailure
{
    ThemeName name;
    ThemeFailure reason;
};

[[nodiscard]] bool operator==(const ThemeLookupFailure &left,
                              const ThemeLookupFailure &right) noexcept;
[[nodiscard]] std::string_view theme_failure_reason(ThemeFailure failure) noexcept;
[[nodiscard]] DisplayText theme_failure_message(const ThemeLookupFailure &failure);
} // namespace nenenib::core
