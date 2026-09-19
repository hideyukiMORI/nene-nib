#pragma once

#include "ThemeChoice.hpp"
#include "ThemeFailure.hpp"

#include <expected>

namespace nenenib::core
{
struct ThemeRecord
{
    ThemeName name;
    std::expected<ThemeChoice, ThemeFailure> value;
};
} // namespace nenenib::core
