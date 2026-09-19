#include "Palette.hpp"

#include "BuiltinThemes.hpp"

namespace nenenib::core
{
Palette palette_for(Appearance appearance) noexcept
{
    return theme_of(theme_for(appearance)).ui;
}
} // namespace nenenib::core
