#pragma once

#include "FilePath.hpp"
#include "SettingsVersion.hpp"

namespace nenenib::adapters::win32
{
struct SettingsPaths
{
    core::FilePath current;
    core::FilePath previous;
    core::FilePath legacy;
};

[[nodiscard]] const core::FilePath &settings_path(const SettingsPaths &paths,
                                                  SettingsVersion version) noexcept;
} // namespace nenenib::adapters::win32
