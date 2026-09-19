#include "ThemeContrast.hpp"

#include <algorithm>
#include <cmath>

namespace nenenib::adapters::win32
{
namespace
{
[[nodiscard]] double channel_luminance(std::uint8_t value) noexcept
{
    const double channel = static_cast<double>(value) / 255.0;
    return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
}

[[nodiscard]] double relative_luminance(core::RgbColor color) noexcept
{
    return 0.2126 * channel_luminance(color.red) + 0.7152 * channel_luminance(color.green) +
           0.0722 * channel_luminance(color.blue);
}

[[nodiscard]] double contrast(core::RgbColor first, core::RgbColor second) noexcept
{
    const auto one = relative_luminance(first) + 0.05;
    const auto other = relative_luminance(second) + 0.05;
    return std::max(one, other) / std::min(one, other);
}
} // namespace

bool theme_has_contrast(const core::Theme &theme) noexcept
{
    return contrast(theme.body.foreground, theme.body.background) >= 4.5 &&
           contrast(theme.ui.text, theme.ui.background) >= 4.5;
}
} // namespace nenenib::adapters::win32
