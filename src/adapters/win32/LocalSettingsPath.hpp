#pragma once

#include "FilePath.hpp"
#include "SettingsFailure.hpp"

#include <expected>

namespace nenenib::adapters::win32
{
[[nodiscard]] std::expected<core::FilePath, application::SettingsFailure> local_settings_path();
} // namespace nenenib::adapters::win32
