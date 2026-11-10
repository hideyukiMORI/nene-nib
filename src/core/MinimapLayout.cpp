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
    // 出ていなければ配置を計算しない（ADR 0171 の決定 1 の追補・オフの費用は予測の当たる分岐 1
    // つ）。
    if (!shown)
    {
        return MinimapLayout{LayoutRect{client.right, client.top, client.right, client.top}, client,
                             0};
    }
    const BodyLayout whole = body_layout(width_of(client), height_of(client), dpi, font_size);
    const std::int32_t width =
        std::clamp(to_pixels(minimap_width_dips, dpi), 0, std::max(width_of(client), 0));
    const LayoutRect panel{client.right - width, client.top + whole.band.top, client.right,
                           client.top + whole.band.bottom};
    LayoutRect body = client;
    body.right -= width;
    const auto rows = static_cast<std::size_t>(std::max(height_of(panel), 0) / minimap_row_pixels);
    return MinimapLayout{panel, body, rows};
}
} // namespace nenenib::core
