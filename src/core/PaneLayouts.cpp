#include "PaneLayouts.hpp"

#include "DevicePixels.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::int32_t divider_dips = 1;
} // namespace

PaneLayouts pane_layouts(LayoutRect client, std::uint32_t dpi, FontSize font_size,
                         SplitLayout layout) noexcept
{
    const BodyLayout whole = body_layout(width_of(client), height_of(client), dpi, font_size);
    const LayoutRect band = whole.band;
    const std::int32_t divider = std::max(to_pixels(divider_dips, dpi), 1);
    switch (layout)
    {
    case SplitLayout::none:
        return PaneLayouts{whole, std::nullopt, std::nullopt};
    case SplitLayout::vertical:
    {
        const std::int32_t each = std::max(width_of(band) - divider, 0) / 2;
        const LayoutRect first{band.left, band.top, band.left + each, band.bottom};
        const LayoutRect second{band.right - each, band.top, band.right, band.bottom};
        return PaneLayouts{body_layout_in(first, dpi, font_size),
                           body_layout_in(second, dpi, font_size),
                           LayoutRect{first.right, band.top, second.left, band.bottom}};
    }
    case SplitLayout::horizontal:
    {
        const std::int32_t each = std::max(height_of(band) - divider, 0) / 2;
        const LayoutRect first{band.left, band.top, band.right, band.top + each};
        const LayoutRect second{band.left, band.bottom - each, band.right, band.bottom};
        return PaneLayouts{body_layout_in(first, dpi, font_size),
                           body_layout_in(second, dpi, font_size),
                           LayoutRect{band.left, first.bottom, band.right, second.top}};
    }
    }
    std::unreachable();
}

BodyLayout active_pane_layout(const PaneLayouts &layouts, PaneSide active) noexcept
{
    switch (active)
    {
    case PaneSide::first:
        return layouts.first;
    case PaneSide::second:
        return layouts.second.value_or(layouts.first);
    }
    std::unreachable();
}

std::optional<BodyLayout> other_pane_layout(const PaneLayouts &layouts, PaneSide active) noexcept
{
    if (!layouts.second.has_value())
    {
        return std::nullopt;
    }
    switch (active)
    {
    case PaneSide::first:
        return layouts.second;
    case PaneSide::second:
        return layouts.first;
    }
    std::unreachable();
}
} // namespace nenenib::core
