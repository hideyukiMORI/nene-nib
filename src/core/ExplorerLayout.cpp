#include "ExplorerLayout.hpp"

#include "BodyLayout.hpp"
#include "DevicePixels.hpp"

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
// 行と部品の寸法（採用案 B・#1390・施主決定 D81）。どれも DIP の固定値で、字の大きさに依らない。
constexpr std::int32_t line_height_dips = 22;
constexpr std::int32_t root_height_dips = 30;
constexpr std::int32_t indent_dips = 12;
constexpr std::int32_t cell_dips = 16;
constexpr std::int32_t inset_dips = 6;
constexpr std::int32_t padding_dips = 4;
constexpr std::int32_t root_padding_dips = 12;
constexpr std::int32_t gap_dips = 6;
constexpr std::int32_t radius_dips = 5;
constexpr std::int32_t corner_dips = 8;
constexpr std::int32_t dot_dips = 6;
// 未保存の丸の右端と行の丸い面の右端の間（採用案のモックの右の余白 4 DIP と丸の外の 10 DIP）。
constexpr std::int32_t dot_inset_dips = 14;
constexpr std::int32_t border_dips = 1;

// 区間 [left, right] に収めた値（right が left より小さければ left）。
[[nodiscard]] constexpr std::int32_t within(std::int32_t value, std::int32_t left,
                                            std::int32_t right) noexcept
{
    return std::max(std::min(value, right), left);
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

ExplorerLayout explorer_layout(LayoutRect client, std::uint32_t dpi,
                               std::optional<ExplorerWidth> shown) noexcept
{
    ExplorerLayout layout{.panel = std::nullopt,
                          .body = client,
                          .line_height = to_pixels(line_height_dips, dpi),
                          .root_height = to_pixels(root_height_dips, dpi),
                          .indent = to_pixels(indent_dips, dpi),
                          .cell = to_pixels(cell_dips, dpi),
                          .inset = to_pixels(inset_dips, dpi),
                          .padding = to_pixels(padding_dips, dpi),
                          .root_padding = to_pixels(root_padding_dips, dpi),
                          .gap = to_pixels(gap_dips, dpi),
                          .radius = to_pixels(radius_dips, dpi),
                          .corner = to_pixels(corner_dips, dpi),
                          .dot = to_pixels(dot_dips, dpi),
                          .dot_inset = to_pixels(dot_inset_dips, dpi),
                          .border = std::max(to_pixels(border_dips, dpi), 1)};
    if (!shown.has_value())
    {
        return layout;
    }
    const LayoutRect band = body_band(width_of(client), height_of(client), dpi);
    const std::int32_t width = explorer_panel_width(width_of(client), dpi, shown.value());
    layout.panel = LayoutRect{client.left, client.top + band.top, client.left + width,
                              client.top + band.bottom};
    layout.body.left = client.left + width;
    return layout;
}

std::size_t explorer_panel_lines(LayoutRect client, std::uint32_t dpi) noexcept
{
    const ExplorerLayout layout = explorer_layout(client, dpi, ExplorerWidth{});
    if (!layout.panel.has_value() || layout.line_height <= 0)
    {
        return 0;
    }
    const std::int32_t height = height_of(layout.panel.value());
    if (height < layout.root_height)
    {
        return 0;
    }
    return 1 + static_cast<std::size_t>((height - layout.root_height) / layout.line_height);
}

LayoutRect explorer_line(const ExplorerLayout &layout, std::size_t index) noexcept
{
    if (!layout.panel.has_value())
    {
        return LayoutRect{0, 0, 0, 0};
    }
    const LayoutRect &panel = layout.panel.value();
    const std::int32_t right = std::max(panel.left, panel.right - layout.border);
    if (index == 0)
    {
        return LayoutRect{panel.left, panel.top, right, panel.top + layout.root_height};
    }
    // 行の番号は面の高さを超えて数えない（桁あふれしない・面の下の行は描かない）。
    const auto rows = static_cast<std::int32_t>(
        std::min<std::size_t>(index - 1, static_cast<std::size_t>(std::max(height_of(panel), 0))));
    const std::int32_t top = panel.top + layout.root_height + (rows * layout.line_height);
    return LayoutRect{panel.left, top, right, top + layout.line_height};
}

std::optional<std::size_t> explorer_line_at(const ExplorerLayout &layout, std::int32_t x,
                                            std::int32_t y) noexcept
{
    if (!layout.panel.has_value() || !contains(layout.panel.value(), x, y) ||
        layout.line_height <= 0)
    {
        return std::nullopt;
    }
    const std::int32_t below = y - layout.panel.value().top - layout.root_height;
    if (below < 0)
    {
        return std::size_t{0};
    }
    return 1 + static_cast<std::size_t>(below / layout.line_height);
}

LayoutRect explorer_root_name(const ExplorerLayout &layout, const LayoutRect &line) noexcept
{
    const std::int32_t left = within(line.left + layout.root_padding, line.left, line.right);
    return LayoutRect{left, line.top, within(line.right - layout.root_padding, left, line.right),
                      line.bottom};
}

ExplorerRowParts explorer_row_parts(const ExplorerLayout &layout, const LayoutRect &line,
                                    std::size_t depth, bool unsaved) noexcept
{
    const std::int32_t surface_left = within(line.left + layout.inset, line.left, line.right);
    const std::int32_t right = within(line.right - layout.inset, surface_left, line.right);
    // 深さは面の幅を超えて数えない（深い木で桁あふれしない）。
    const auto steps = static_cast<std::int32_t>(
        std::min<std::size_t>(depth, static_cast<std::size_t>(std::max(right - surface_left, 0))));
    const std::int32_t chevron_left =
        within(surface_left + layout.padding + (steps * layout.indent), surface_left, right);
    const std::int32_t cell_top = line.top + ((height_of(line) - layout.cell) / 2);
    const auto cell_at = [&](std::int32_t left)
    {
        return LayoutRect{left, cell_top, within(left + layout.cell, left, right),
                          cell_top + layout.cell};
    };
    const LayoutRect chevron = cell_at(chevron_left);
    const LayoutRect mark = cell_at(chevron.right);
    const std::int32_t name_left = within(mark.right + layout.gap, mark.right, right);
    LayoutRect dot{right, line.top, right, line.top};
    std::int32_t name_right = within(right - layout.padding, name_left, right);
    if (unsaved)
    {
        const std::int32_t dot_right = within(right - layout.dot_inset, name_left, right);
        const std::int32_t dot_left = within(dot_right - layout.dot, name_left, dot_right);
        const std::int32_t dot_top = line.top + ((height_of(line) - layout.dot) / 2);
        dot = LayoutRect{dot_left, dot_top, dot_right, dot_top + layout.dot};
        name_right = within(dot_left - layout.gap, name_left, dot_left);
    }
    return ExplorerRowParts{LayoutRect{surface_left, line.top, right, line.bottom}, chevron, mark,
                            LayoutRect{name_left, line.top, name_right, line.bottom}, dot};
}

std::int32_t explorer_guide_x(const ExplorerLayout &layout, const LayoutRect &line,
                              std::size_t level) noexcept
{
    const LayoutRect chevron = explorer_row_parts(layout, line, level, false).chevron;
    return chevron.left + ((width_of(chevron) - layout.border) / 2);
}
} // namespace nenenib::core
