#include "PaneLayouts.hpp"

#include "DevicePixels.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::int32_t divider_dips = 1;
// 境界の当たりの広さ（面の境の ExplorerLayout と同じ 3 DIP）。
constexpr std::int32_t divider_grab_dips = 3;
// どちらの枠にも残す最小（ADR 0148 の 2026-11-22 の追補の決定 2）。桁の幅は字の大きさの 0.6 em を
// 見積もる（core は字幅を測れない）。
constexpr std::int32_t minimum_columns = 8;
constexpr std::int32_t minimum_rows = 2;
constexpr float column_em = 0.6F;

// client の中の本文の帯。縦（タブの帯の下からステータスバーの上まで）と横を client の原点から
// 測る。エクスプローラーの面が左を取ると client の左端は 0 でない（ADR 0162 の決定 2）。
[[nodiscard]] LayoutRect body_band(LayoutRect client, std::uint32_t dpi,
                                   FontSize font_size) noexcept
{
    const LayoutRect band = body_layout(width_of(client), height_of(client), dpi, font_size).band;
    return LayoutRect{client.left + band.left, client.top + band.top, client.left + band.right,
                      client.top + band.bottom};
}

[[nodiscard]] std::int32_t divider_pixels(std::uint32_t dpi) noexcept
{
    return std::max(to_pixels(divider_dips, dpi), 1);
}

// 桁 1 つの見積もり（DIP・切り上げ）。libm を呼ばない（ARC-003）。
[[nodiscard]] std::int32_t column_dips(FontSize font_size) noexcept
{
    const float raw = font_size_dips(font_size) * column_em;
    const auto whole = static_cast<std::int32_t>(raw);
    return static_cast<float>(whole) < raw ? whole + 1 : whole;
}

// 帯全体を 1 枠で置いた配置 whole から、分割の向きの寸法を作る（first は 0 のまま）。
[[nodiscard]] SplitExtent extent_of(const BodyLayout &whole, std::uint32_t dpi, FontSize font_size,
                                    PaneDimension dimension) noexcept
{
    const LayoutRect band = whole.band;
    switch (dimension)
    {
    case PaneDimension::width:
    {
        const auto column = to_pixels(column_dips(font_size), dpi);
        const auto minimum = (whole.content.left - band.left) + minimum_columns * column;
        return SplitExtent{dimension, band.left, std::max(width_of(band) - divider_pixels(dpi), 0),
                           0, minimum};
    }
    case PaneDimension::height:
    {
        const auto minimum = (whole.content.top - band.top) + minimum_rows * whole.line_height;
        return SplitExtent{dimension, band.top, std::max(height_of(band) - divider_pixels(dpi), 0),
                           0, minimum};
    }
    }
    std::unreachable();
}

[[nodiscard]] bool fits_two(const SplitExtent &extent) noexcept
{
    return extent.available > 0 && extent.available >= 2 * extent.minimum;
}

[[nodiscard]] SplitShare clamped_share(const SplitExtent &extent, std::int64_t first) noexcept
{
    if (!fits_two(extent))
    {
        return SplitShare{};
    }
    return SplitShare{static_cast<std::int32_t>(std::clamp<std::int64_t>(
                          first, extent.minimum, extent.available - extent.minimum)),
                      extent.available};
}

// 帯を向きと比 split で first の長さと残りに分ける。等分は今までの形（second は帯の終わりから
// 同じ長さ・余りは境界）、等分でなければ境界はちょうど 1 DIP。
[[nodiscard]] PaneLayouts placed(const BodyLayout &whole, std::uint32_t dpi, FontSize font_size,
                                 std::pair<PaneDimension, SplitShare> split) noexcept
{
    const auto [dimension, share] = split;
    const auto extent = extent_of(whole, dpi, font_size, dimension);
    const bool even = is_even_share(share) || !fits_two(extent);
    const auto length = split_first_length(extent, share);
    const LayoutRect band = whole.band;
    switch (dimension)
    {
    case PaneDimension::width:
    {
        const LayoutRect first{band.left, band.top, band.left + length, band.bottom};
        const auto start = even ? band.right - length : first.right + divider_pixels(dpi);
        const LayoutRect second{start, band.top, band.right, band.bottom};
        return PaneLayouts{body_layout_in(first, dpi, font_size),
                           body_layout_in(second, dpi, font_size),
                           LayoutRect{first.right, band.top, second.left, band.bottom}};
    }
    case PaneDimension::height:
    {
        const LayoutRect first{band.left, band.top, band.right, band.top + length};
        const auto start = even ? band.bottom - length : first.bottom + divider_pixels(dpi);
        const LayoutRect second{band.left, start, band.right, band.bottom};
        return PaneLayouts{body_layout_in(first, dpi, font_size),
                           body_layout_in(second, dpi, font_size),
                           LayoutRect{band.left, first.bottom, band.right, second.top}};
    }
    }
    std::unreachable();
}

// 鍵の 1 回の後の操作中の枠の長さ（own は今の長さ・step は升の数ぶんの画素）。
[[nodiscard]] std::int64_t resized_length(PaneResize resize, std::int64_t own, std::int64_t step,
                                          std::int64_t available) noexcept
{
    switch (resize)
    {
    case PaneResize::grow:
        return own + step;
    case PaneResize::shrink:
        return own - step;
    case PaneResize::set:
        return step;
    case PaneResize::maximize:
        return available;
    case PaneResize::even:
        return available / 2;
    }
    std::unreachable();
}
} // namespace

PaneLayouts pane_layouts(LayoutRect client, std::uint32_t dpi, FontSize font_size,
                         PaneSplit split) noexcept
{
    const BodyLayout whole = body_layout_in(body_band(client, dpi, font_size), dpi, font_size);
    const auto dimension = split_dimension(split.layout);
    if (!dimension.has_value())
    {
        return PaneLayouts{whole, std::nullopt, std::nullopt};
    }
    return placed(whole, dpi, font_size, {dimension.value(), split.share});
}

std::optional<SplitExtent> split_extent(LayoutRect client, std::uint32_t dpi, FontSize font_size,
                                        PaneSplit split) noexcept
{
    const auto dimension = split_dimension(split.layout);
    if (!dimension.has_value())
    {
        return std::nullopt;
    }
    const BodyLayout whole = body_layout_in(body_band(client, dpi, font_size), dpi, font_size);
    auto extent = extent_of(whole, dpi, font_size, dimension.value());
    extent.first = split_first_length(extent, split.share);
    return extent;
}

std::int32_t split_first_length(const SplitExtent &extent, SplitShare share) noexcept
{
    if (is_even_share(share) || !fits_two(extent))
    {
        return extent.available / 2;
    }
    const auto wanted = static_cast<std::int64_t>(extent.available) * share.first / share.whole;
    return static_cast<std::int32_t>(
        std::clamp<std::int64_t>(wanted, extent.minimum, extent.available - extent.minimum));
}

SplitShare split_share_at(const SplitExtent &extent, std::int32_t at) noexcept
{
    return clamped_share(extent, static_cast<std::int64_t>(at) - extent.origin);
}

std::optional<SplitShare> sized_share(const SplitExtent &extent, PaneSide active, PaneSizing sizing,
                                      std::int32_t cell) noexcept
{
    if (sizing.resize == PaneResize::even)
    {
        return SplitShare{};
    }
    if (sizing.dimension != extent.dimension)
    {
        return std::nullopt;
    }
    const std::int64_t step = static_cast<std::int64_t>(std::max(cell, 1)) * sizing.cells;
    const std::int64_t own =
        active == PaneSide::first ? extent.first : extent.available - extent.first;
    const auto wanted = resized_length(sizing.resize, own, step, extent.available);
    return clamped_share(extent, active == PaneSide::first ? wanted : extent.available - wanted);
}

bool split_divider_hit(const PaneLayouts &layouts, std::uint32_t dpi, std::int32_t x,
                       std::int32_t y) noexcept
{
    if (!layouts.divider.has_value() || !layouts.second.has_value())
    {
        return false;
    }
    const LayoutRect divider = layouts.divider.value();
    const std::int32_t grab = std::max(to_pixels(divider_grab_dips, dpi), 1);
    if (layouts.second.value().band.top > layouts.first.band.top)
    {
        return x >= divider.left && x < divider.right && y >= divider.top - grab &&
               y < divider.bottom + grab;
    }
    return x >= divider.left - grab && x < divider.right + grab && y >= divider.top &&
           y < divider.bottom;
}

std::optional<PaneDimension> split_dimension(SplitLayout layout) noexcept
{
    switch (layout)
    {
    case SplitLayout::none:
        return std::nullopt;
    case SplitLayout::vertical:
        return PaneDimension::width;
    case SplitLayout::horizontal:
        return PaneDimension::height;
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
