#pragma once

#include "FilePort.hpp"
#include "ThemeDocument.hpp"
#include "ThemeFailure.hpp"

#include <cstddef>
#include <expected>
#include <string_view>

namespace nenenib::adapters::win32
{
inline constexpr std::size_t maximum_theme_bytes = 16U * 1024U;

[[nodiscard]] std::expected<core::ThemeName, core::ThemeFailure>
theme_name_for_file(const core::FilePath &path);

[[nodiscard]] std::expected<core::ThemeDocument, core::ThemeFailure>
decode_theme(std::string_view bytes, const core::ThemeName &expected_name);
[[nodiscard]] std::expected<core::ThemeDocument, core::ThemeFailure>
load_theme(application::FilePort &files, const core::FilePath &path);
} // namespace nenenib::adapters::win32
