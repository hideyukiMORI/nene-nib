#include "StatusBarLayout.hpp"

#include "DevicePixels.hpp"

#include <algorithm>
#include <cstddef>

namespace nenenib::core
{
namespace
{
constexpr std::int32_t band_height_dips = 28;
constexpr std::int32_t side_padding_dips = 12;
constexpr std::int32_t item_gap_dips = 16;
constexpr std::int32_t toggle_padding_dips = 2;
constexpr std::int32_t segment_width_dips = 44;
constexpr std::int32_t segment_height_dips = 20;
constexpr std::int32_t mode_width_dips = 72;
constexpr std::int32_t corner_radius_dips = 6;
constexpr std::int32_t segment_radius_dips = 4;
// 右の 3 項目の幅（行と桁・文字コード・改行）。左から右へ並べた順に対応する。
// 文字コードは「UTF-8 BOM」「Shift_JIS」まで入る幅が要る（ADR 0010 の決定 14 で 44 → 72）。
constexpr std::array<std::int32_t, status_item_count> item_width_dips{96, 72, 36};

[[nodiscard]] LayoutRect toggle_box(std::int32_t band_top, std::int32_t band_height,
                                    std::uint32_t dpi) noexcept
{
    const std::int32_t padding = to_pixels(toggle_padding_dips, dpi);
    const std::int32_t width = to_pixels(segment_width_dips, dpi) * 2 + padding * 3;
    const std::int32_t height = to_pixels(segment_height_dips, dpi) + padding * 2;
    const std::int32_t left = to_pixels(side_padding_dips, dpi);
    const std::int32_t top = band_top + (band_height - height) / 2;
    return LayoutRect{left, top, left + width, top + height};
}

[[nodiscard]] std::array<LayoutRect, status_item_count>
item_rects(std::int32_t width, const LayoutRect &band, std::uint32_t dpi) noexcept
{
    std::array<LayoutRect, status_item_count> rectangles{};
    std::int32_t right = width - to_pixels(side_padding_dips, dpi);
    for (std::size_t step = 0; step < status_item_count; ++step)
    {
        const std::size_t index = status_item_count - 1 - step;
        const std::int32_t item = to_pixels(item_width_dips.at(index), dpi);
        rectangles.at(index) = LayoutRect{right - item, band.top, right, band.bottom};
        right -= item + to_pixels(item_gap_dips, dpi);
    }
    return rectangles;
}
} // namespace

StatusBarLayout status_bar_layout(std::int32_t width, std::int32_t height,
                                  std::uint32_t dpi) noexcept
{
    const std::int32_t band_height = to_pixels(band_height_dips, dpi);
    const std::int32_t top = std::max(height - band_height, 0);
    const LayoutRect band{0, top, width, height};
    const LayoutRect toggle = toggle_box(top, height - top, dpi);
    const std::int32_t padding = to_pixels(toggle_padding_dips, dpi);
    const std::int32_t segment = to_pixels(segment_width_dips, dpi);
    const LayoutRect ordinary{toggle.left + padding, toggle.top + padding,
                              toggle.left + padding + segment, toggle.bottom - padding};
    const std::int32_t vim_left = ordinary.right + padding;
    const std::int32_t mode_left = toggle.right + to_pixels(item_gap_dips, dpi);
    return StatusBarLayout{
        .band = band,
        .toggle = toggle,
        .toggle_ordinary = ordinary,
        .toggle_vim = LayoutRect{vim_left, ordinary.top, vim_left + segment, ordinary.bottom},
        .mode = LayoutRect{mode_left, band.top, mode_left + to_pixels(mode_width_dips, dpi),
                           band.bottom},
        .items = item_rects(width, band, dpi),
        .corner_radius = to_pixels(corner_radius_dips, dpi),
        .segment_radius = to_pixels(segment_radius_dips, dpi)};
}

StatusBarHit status_bar_hit(const StatusBarLayout &layout, std::int32_t x, std::int32_t y) noexcept
{
    if (contains(layout.toggle_ordinary, x, y))
    {
        return StatusBarHit::toggle_ordinary;
    }
    if (contains(layout.toggle_vim, x, y))
    {
        return StatusBarHit::toggle_vim;
    }
    return StatusBarHit::none;
}
} // namespace nenenib::core
