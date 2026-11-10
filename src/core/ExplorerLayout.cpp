#include "ExplorerLayout.hpp"

#include "BodyLayout.hpp"
#include "DevicePixels.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <cstddef>

namespace nenenib::core
{
namespace
{
constexpr std::int32_t panel_width_dips = 240;
constexpr std::int32_t panel_width_percent = 40;
// 使う人が決めた幅の上限（client の 60%）と、本文に残す幅（D78）。
constexpr std::int32_t chosen_width_percent = 60;
constexpr std::int32_t body_reserve_dips = 200;
// 境の当たりの片側（D78）。
constexpr std::int32_t divider_grab_dips = 3;
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

// 使う人が決めた幅（物理画素）を範囲に切り詰める 1 本（D78）。explorer_panel_width と
// explorer_dragged_width が共用する。
[[nodiscard]] std::int32_t clamped_panel_pixels(std::int32_t pixels, std::int32_t client_width,
                                                std::uint32_t dpi) noexcept
{
    const std::int32_t maximum = std::min(client_width * chosen_width_percent / 100,
                                          client_width - to_pixels(body_reserve_dips, dpi));
    const std::int32_t minimum = to_pixels(explorer_width_minimum_dips, dpi);
    return std::max(std::min(std::max(pixels, minimum), maximum), 0);
}
} // namespace

std::int32_t explorer_panel_width(std::int32_t client_width, std::uint32_t dpi,
                                  ExplorerWidth width) noexcept
{
    if (!width.dips.has_value())
    {
        return std::max(
            std::min(to_pixels(panel_width_dips, dpi), client_width * panel_width_percent / 100),
            0);
    }
    return clamped_panel_pixels(to_pixels(explorer_width_clamped(width.dips.value()), dpi),
                                client_width, dpi);
}

std::int32_t explorer_width_clamped(std::int32_t dips) noexcept
{
    return std::clamp(dips, explorer_width_minimum_dips, explorer_width_limit_dips);
}

std::int32_t explorer_dragged_width(std::int32_t client_width, std::uint32_t dpi,
                                    std::int32_t panel_pixels) noexcept
{
    const std::int32_t dips = to_dips(clamped_panel_pixels(panel_pixels, client_width, dpi), dpi);
    return explorer_width_clamped(dips);
}

bool explorer_divider_hit(const ExplorerLayout &layout, std::uint32_t dpi, std::int32_t x,
                          std::int32_t y) noexcept
{
    if (!layout.panel.has_value() || width_of(layout.panel.value()) <= 0)
    {
        return false;
    }
    const LayoutRect &panel = layout.panel.value();
    const std::int32_t grab = std::max(to_pixels(divider_grab_dips, dpi), 1);
    return x >= panel.right - grab && x < panel.right + grab && y >= panel.top && y < panel.bottom;
}

std::optional<std::int32_t> explorer_width_of(std::string_view text) noexcept
{
    constexpr std::size_t digits_limit = 4;
    constexpr std::int32_t decimal_base = 10;
    if (text.empty() || text.size() > digits_limit || text.front() == '0')
    {
        return std::nullopt;
    }
    std::int32_t value = 0;
    for (const char digit : text)
    {
        if (digit < '0' || digit > '9')
        {
            return std::nullopt;
        }
        value = value * decimal_base + static_cast<std::int32_t>(digit - '0');
    }
    if (value < explorer_width_minimum_dips || value > explorer_width_limit_dips)
    {
        return std::nullopt;
    }
    return value;
}

ExplorerLayout explorer_layout(LayoutRect client, std::uint32_t dpi, FontSize font_size,
                               std::optional<ExplorerWidth> shown) noexcept
{
    const BodyLayout whole = body_layout(width_of(client), height_of(client), dpi, font_size);
    ExplorerLayout layout{.panel = std::nullopt,
                          .body = client,
                          .line_height = whole.line_height,
                          .indent = to_pixels(indent_dips, dpi),
                          .marker = marker_width(dpi, font_size),
                          .padding = to_pixels(padding_dips, dpi),
                          .border = std::max(to_pixels(border_dips, dpi), 1)};
    if (!shown.has_value())
    {
        return layout;
    }
    const std::int32_t width = explorer_panel_width(width_of(client), dpi, shown.value());
    layout.panel = LayoutRect{client.left, client.top + whole.band.top, client.left + width,
                              client.top + whole.band.bottom};
    layout.body.left = client.left + width;
    return layout;
}

std::size_t explorer_panel_lines(LayoutRect client, std::uint32_t dpi, FontSize font_size) noexcept
{
    const ExplorerLayout layout = explorer_layout(client, dpi, font_size, ExplorerWidth{});
    if (!layout.panel.has_value() || layout.line_height <= 0)
    {
        return 0;
    }
    return static_cast<std::size_t>(std::max(height_of(layout.panel.value()), 0) /
                                    layout.line_height);
}
} // namespace nenenib::core
