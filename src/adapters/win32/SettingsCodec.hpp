#pragma once

#include "EditorSettings.hpp"
#include "SettingsFailure.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
[[nodiscard]] std::expected<core::EditorSettings, application::SettingsFailure>
decode_settings(std::string_view bytes);
[[nodiscard]] std::string encode_settings(const core::EditorSettings &settings);
} // namespace nenenib::adapters::win32
