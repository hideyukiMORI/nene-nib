#pragma once

#include <cstdint>

namespace nenenib::application
{
enum class ThemeFailure : std::uint8_t
{
    not_found,
    unreadable,
    too_large,
    malformed,
    duplicate_key,
    unknown_key,
    missing_field,
    unsupported_version,
    invalid_name,
    name_mismatch,
    reserved_name,
    invalid_text,
    invalid_appearance,
    invalid_color,
    insufficient_contrast
};
} // namespace nenenib::application
