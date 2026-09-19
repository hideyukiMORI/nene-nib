#pragma once

#include "ThemeCatalog.hpp"

#include <optional>

namespace nenenib::application
{
struct ThemeInventory
{
    core::ThemeCatalog catalog;
    std::optional<core::DisplayText> notice;
};
} // namespace nenenib::application
