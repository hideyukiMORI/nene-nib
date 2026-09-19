#pragma once

#include "RgbColor.hpp"
#include "RgbaColor.hpp"
#include "ThemeFailure.hpp"

#include <expected>
#include <string_view>

namespace nenenib::adapters::win32
{
[[nodiscard]] std::expected<core::RgbColor, application::ThemeFailure>
decode_rgb(std::string_view text);
[[nodiscard]] std::expected<core::RgbaColor, application::ThemeFailure>
decode_rgba(std::string_view text);
} // namespace nenenib::adapters::win32
