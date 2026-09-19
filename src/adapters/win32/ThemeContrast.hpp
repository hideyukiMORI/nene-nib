#pragma once

#include "Theme.hpp"

namespace nenenib::adapters::win32
{
[[nodiscard]] bool theme_has_contrast(const core::Theme &theme) noexcept;
} // namespace nenenib::adapters::win32
