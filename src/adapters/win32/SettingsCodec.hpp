#pragma once

#include "EditorSettings.hpp"
#include "SettingsIssue.hpp"
#include "ThemeCatalog.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
[[nodiscard]] std::expected<core::EditorSettings, application::SettingsIssue>
decode_settings(std::string_view bytes,
                const core::ThemeCatalog &themes = core::ThemeCatalog::builtins());
[[nodiscard]] std::string encode_settings(const core::EditorSettings &settings);
} // namespace nenenib::adapters::win32
