#include "BodyLayout.hpp"

#include "DevicePixels.hpp"
#include "StatusBarLayout.hpp"
#include "TitleBarLayout.hpp"

#include <algorithm>

namespace nenenib::core
{
namespace
{
constexpr std::int32_t body_top_dips = 12;
constexpr std::int32_t line_height_dips = 24;
constexpr std::int32_t gutter_width_dips = 56;
constexpr std::int32_t caret_width_dips = 2;
constexpr std::size_t single_tab = 1;
} // namespace

BodyLayout body_layout(std::int32_t width, std::int32_t height, std::uint32_t dpi) noexcept
{
    const auto title = title_bar_layout(width, dpi, single_tab);
    const auto status = status_bar_layout(width, height, dpi);
    const std::int32_t top = std::min(title.band.bottom, status.band.top);
    const LayoutRect band{0, top, width, status.band.top};
    const std::int32_t first = band.top + to_pixels(body_top_dips, dpi);
    const std::int32_t line_height = std::max(to_pixels(line_height_dips, dpi), 1);
    const std::int32_t gutter = to_pixels(gutter_width_dips, dpi);
    const std::int32_t rows = std::max((band.bottom - first) / line_height, 0);
    return BodyLayout{.band = band,
                      .gutter = LayoutRect{band.left, first, band.left + gutter, band.bottom},
                      .content = LayoutRect{band.left + gutter, first, band.right, band.bottom},
                      .line_height = line_height,
                      .caret_width = to_pixels(caret_width_dips, dpi),
                      .visible_lines = static_cast<std::size_t>(rows)};
}

LayoutRect body_line_rect(const BodyLayout &layout, std::size_t index) noexcept
{
    const std::int32_t top =
        layout.content.top + layout.line_height * static_cast<std::int32_t>(index);
    return LayoutRect{layout.band.left, top, layout.band.right, top + layout.line_height};
}
} // namespace nenenib::core
