#pragma once

#include <cstdint>

namespace nenenib::application
{
enum class SettingsFailure : std::uint8_t
{
    location_unavailable,
    unreadable,
    too_large,
    malformed,
    unsupported_version,
    unknown_theme,
    invalid_font_family,
    invalid_font_size,
    unwritable,
    changed_externally,
    not_loaded
};
} // namespace nenenib::application
