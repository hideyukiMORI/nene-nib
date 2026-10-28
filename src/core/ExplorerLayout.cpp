#include "ExplorerLayout.hpp"

#include "BodyLayout.hpp"
#include "DevicePixels.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>

namespace nenenib::core
{
namespace
{
constexpr std::int32_t panel_width_dips = 240;
constexpr std::int32_t panel_width_percent = 40;
constexpr std::int32_t indent_dips = 12;
constexpr std::int32_t padding_dips = 8;
constexpr std::int32_t border_dips = 1;
// 印の字（字体に依らない幾何学の三角・ADR 0162 の決定 2）。
constexpr char32_t closed_marker = U'▸';
constexpr char32_t open_marker = U'▾';

// 印の升の幅。2 つの印の桁の多い方 × 字の大きさ（DIP を四捨五入）。
[[nodiscard]] std::int32_t marker_width(std::uint32_t dpi, FontSize font_size) noexcept
{
    const auto columns = static_cast<std::int32_t>(
        std::max(column_advance(closed_marker, 0), column_advance(open_marker, 0)));
    const auto em_dips = static_cast<std::int32_t>(font_size_dips(font_size) + 0.5F);
    return columns * to_pixels(em_dips, dpi);
}
} // namespace

ExplorerLayout explorer_layout(LayoutRect client, std::uint32_t dpi, FontSize font_size,
                               bool shown) noexcept
{
    const BodyLayout whole = body_layout(width_of(client), height_of(client), dpi, font_size);
    ExplorerLayout layout{.panel = std::nullopt,
                          .body = client,
                          .line_height = whole.line_height,
                          .indent = to_pixels(indent_dips, dpi),
                          .marker = marker_width(dpi, font_size),
                          .padding = to_pixels(padding_dips, dpi),
                          .border = std::max(to_pixels(border_dips, dpi), 1)};
    if (!shown)
    {
        return layout;
    }
    const std::int32_t width = std::max(
        std::min(to_pixels(panel_width_dips, dpi), width_of(client) * panel_width_percent / 100),
        0);
    layout.panel = LayoutRect{client.left, client.top + whole.band.top, client.left + width,
                              client.top + whole.band.bottom};
    layout.body.left = client.left + width;
    return layout;
}
} // namespace nenenib::core
