#include "MinimapLayout.hpp"

#include "BodyLayout.hpp"
#include "DevicePixels.hpp"
#include "Minimap.hpp"

#include <algorithm>

namespace nenenib::core
{
MinimapLayout minimap_layout(LayoutRect client, std::uint32_t dpi, FontSize font_size,
                             bool shown) noexcept
{
    const BodyLayout whole = body_layout(width_of(client), height_of(client), dpi, font_size);
    const std::int32_t width =
        std::clamp(to_pixels(minimap_width_dips, dpi), 0, std::max(width_of(client), 0)) *
        static_cast<std::int32_t>(shown);
    const LayoutRect panel{client.right - width, client.top + whole.band.top, client.right,
                           client.top + whole.band.bottom};
    LayoutRect body = client;
    body.right -= width;
    const auto rows = static_cast<std::size_t>(std::max(height_of(panel), 0) / minimap_row_pixels);
    return MinimapLayout{panel, body, rows};
}
} // namespace nenenib::core
