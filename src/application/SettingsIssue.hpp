#pragma once

#include "SettingsFailure.hpp"
#include "ThemeLookupFailure.hpp"

#include <variant>

namespace nenenib::application
{
using SettingsIssue = std::variant<SettingsFailure, core::ThemeLookupFailure>;
[[nodiscard]] bool operator==(const SettingsIssue &issue, SettingsFailure failure) noexcept;
} // namespace nenenib::application
