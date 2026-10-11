#include "SettingsPaths.hpp"

#include <utility>

namespace nenenib::adapters::win32
{
const core::FilePath &settings_path(const SettingsPaths &paths, SettingsVersion version) noexcept
{
    switch (version)
    {
    case SettingsVersion::v1:
        return paths.legacy;
    case SettingsVersion::v2:
        return paths.previous;
    case SettingsVersion::v3:
        return paths.current;
    }
    std::unreachable();
}
} // namespace nenenib::adapters::win32
