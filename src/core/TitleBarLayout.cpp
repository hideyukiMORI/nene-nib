#include "TitleBarLayout.hpp"

#include "DevicePixels.hpp"

#include <algorithm>

namespace nenenib::core
{
namespace
{
constexpr std::int32_t band_height_dips = 40;
constexpr std::int32_t strip_left_dips = 8;
// タブの下端はタイトルバーの帯の下端と面一（上余白 8 ＋ 高さ 32 ＝ 帯の 40）。
constexpr std::int32_t tab_top_dips = 8;
constexpr std::int32_t tab_height_dips = 32;
constexpr std::int32_t tab_gap_dips = 2;
constexpr std::int32_t tab_minimum_dips = 120;
constexpr std::int32_t tab_maximum_dips = 200;
constexpr std::int32_t add_tab_dips = 32;
constexpr std::int32_t caption_button_dips = 46;
constexpr std::int32_t corner_radius_dips = 6;
constexpr std::int32_t underline_dips = 2;
constexpr std::int32_t glyph_dips = 10;

[[nodiscard]] std::int32_t tab_width_for(std::int32_t free_width, std::uint32_t dpi,
                                         std::size_t tab_count) noexcept
{
    const auto count = static_cast<std::int32_t>(std::max<std::size_t>(tab_count, 1));
    const std::int32_t gap = to_pixels(tab_gap_dips, dpi);
    const std::int32_t share = (free_width - gap * (count - 1)) / count;
    return std::clamp(share, to_pixels(tab_minimum_dips, dpi), to_pixels(tab_maximum_dips, dpi));
}

[[nodiscard]] LayoutRect caption_button(std::int32_t right, std::int32_t height,
                                        std::int32_t button) noexcept
{
    return LayoutRect{right - button, 0, right, height};
}
} // namespace

TitleBarLayout title_bar_layout(std::int32_t width, std::uint32_t dpi,
                                std::size_t tab_count) noexcept
{
    const std::int32_t height = to_pixels(band_height_dips, dpi);
    const std::int32_t button = to_pixels(caption_button_dips, dpi);
    const std::int32_t add_tab = to_pixels(add_tab_dips, dpi);
    const std::int32_t left = to_pixels(strip_left_dips, dpi);
    const std::int32_t top = to_pixels(tab_top_dips, dpi);
    const std::int32_t bottom = top + to_pixels(tab_height_dips, dpi);
    const std::int32_t gap = to_pixels(tab_gap_dips, dpi);
    const std::int32_t free_width = std::max(width - button * 3 - left - add_tab - gap, 0);
    const std::int32_t tab_width = tab_width_for(free_width, dpi, tab_count);
    const auto count = static_cast<std::int32_t>(std::max<std::size_t>(tab_count, 1));
    const std::int32_t strip_right = left + tab_width * count + gap * (count - 1);
    const LayoutRect close = caption_button(width, height, button);
    return TitleBarLayout{
        .band = LayoutRect{0, 0, width, height},
        .tabs = LayoutRect{left, top, strip_right, bottom},
        .add_tab = LayoutRect{strip_right + gap, top, strip_right + gap + add_tab, bottom},
        .minimize = caption_button(close.left - button, height, button),
        .maximize = caption_button(close.left, height, button),
        .close = close,
        .tab_width = tab_width,
        .tab_gap = gap,
        .corner_radius = to_pixels(corner_radius_dips, dpi),
        .underline = to_pixels(underline_dips, dpi),
        .glyph = to_pixels(glyph_dips, dpi),
        .tab_count = std::max<std::size_t>(tab_count, 1)};
}

LayoutRect tab_rect(const TitleBarLayout &layout, std::size_t index) noexcept
{
    const auto step = layout.tab_width + layout.tab_gap;
    const std::int32_t left = layout.tabs.left + step * static_cast<std::int32_t>(index);
    return LayoutRect{left, layout.tabs.top, left + layout.tab_width, layout.tabs.bottom};
}

TitleBarHit title_bar_hit(const TitleBarLayout &layout, std::int32_t x, std::int32_t y) noexcept
{
    if (!contains(layout.band, x, y))
    {
        return TitleBarHit::none;
    }
    if (contains(layout.close, x, y))
    {
        return TitleBarHit::close;
    }
    if (contains(layout.maximize, x, y))
    {
        return TitleBarHit::maximize;
    }
    if (contains(layout.minimize, x, y))
    {
        return TitleBarHit::minimize;
    }
    if (contains(layout.add_tab, x, y))
    {
        return TitleBarHit::add_tab;
    }
    if (contains(layout.tabs, x, y))
    {
        return TitleBarHit::tab;
    }
    return TitleBarHit::caption;
}
} // namespace nenenib::core
