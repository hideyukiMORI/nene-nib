#pragma once

#include "DisplayText.hpp"

#include <cstdint>

namespace nenenib::core
{
enum class ExFailure : std::uint8_t
{
    unknown_command,
    unknown_option,
    unknown_theme,
    invalid_font_size,
    invalid_font_family,
    invalid_text,
    too_long
};

[[nodiscard]] DisplayText ex_failure_message(ExFailure failure);
} // namespace nenenib::core
