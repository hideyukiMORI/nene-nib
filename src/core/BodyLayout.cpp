#include "BodyLayout.hpp"

#include "DevicePixels.hpp"
#include "ScrollFraction.hpp"
#include "StatusBarLayout.hpp"
#include "TitleBarLayout.hpp"

#include <algorithm>
#include <limits>

namespace nenenib::core
{
namespace
{
constexpr std::int32_t body_top_dips = 12;
constexpr std::int32_t gutter_width_dips = 56;
constexpr std::int32_t body_left_padding_dips = 8;
constexpr std::int32_t caret_width_dips = 2;
constexpr std::size_t single_tab = 1;
} // namespace

BodyLayout body_layout(std::int32_t width, std::int32_t height, std::uint32_t dpi,
                       FontSize font_size) noexcept
{
    return body_layout_in(body_band(width, height, dpi), dpi, font_size);
}

LayoutRect body_band(std::int32_t width, std::int32_t height, std::uint32_t dpi) noexcept
{
    const auto title = title_bar_layout(TitleBarInput{width, dpi, single_tab, 0, 0, std::nullopt});
    const auto status = status_bar_layout(width, height, dpi);
    const std::int32_t top = std::min(title.band.bottom, status.band.top);
    return LayoutRect{0, top, width, status.band.top};
}

BodyLayout body_layout_in(LayoutRect band, std::uint32_t dpi, FontSize font_size) noexcept
{
    const std::int32_t width = width_of(band);
    const std::int32_t first = band.top + to_pixels(body_top_dips, dpi);
    const auto line_dips = static_cast<std::int32_t>(font_size_dips(font_size) * 1.6F + 0.5F);
    const auto gutter_dips = static_cast<std::int32_t>(
        static_cast<float>(gutter_width_dips) * font_size_ratio(font_size) + 0.5F);
    const std::int32_t line_height = std::max(to_pixels(line_dips, dpi), 1);
    const std::int32_t gutter = std::clamp(to_pixels(gutter_dips, dpi), 0, std::max(width, 0));
    const auto padding_dips = static_cast<std::int32_t>(
        static_cast<float>(body_left_padding_dips) * font_size_ratio(font_size) + 0.5F);
    const auto content_left =
        std::min(band.left + gutter + to_pixels(padding_dips, dpi), band.right);
    const std::int32_t rows = std::max((band.bottom - first) / line_height, 0);
    return BodyLayout{.band = band,
                      .gutter = LayoutRect{band.left, first, band.left + gutter, band.bottom},
                      .content = LayoutRect{content_left, first, band.right, band.bottom},
                      .line_height = line_height,
                      .caret_width = to_pixels(caret_width_dips, dpi),
                      .visible_lines = static_cast<std::size_t>(rows),
                      .scrolled = 0};
}

LayoutRect body_line_rect(const BodyLayout &layout, std::size_t index) noexcept
{
    const std::int32_t top =
        layout.content.top + layout.line_height * static_cast<std::int32_t>(index);
    return LayoutRect{layout.band.left, top, layout.band.right, top + layout.line_height};
}

BodyLayout scrolled_body(BodyLayout layout, std::size_t fraction) noexcept
{
    const auto units = static_cast<std::int64_t>(scroll_fraction_units);
    const auto part = static_cast<std::int64_t>(std::min(fraction, scroll_fraction_units - 1));
    layout.scrolled =
        static_cast<std::int32_t>(part * std::max<std::int64_t>(layout.line_height, 1) / units);
    return layout;
}

std::int32_t body_row_top(const BodyLayout &layout, std::int64_t row) noexcept
{
    const auto value = static_cast<double>(layout.content.top) - layout.scrolled +
                       (static_cast<double>(row) * layout.line_height);
    return static_cast<std::int32_t>(
        std::clamp(value, static_cast<double>(std::numeric_limits<std::int32_t>::min()) / 2.0,
                   static_cast<double>(std::numeric_limits<std::int32_t>::max()) / 2.0));
}

std::int64_t body_row_at(const BodyLayout &layout, std::int32_t y) noexcept
{
    const auto from = static_cast<std::int64_t>(y) - layout.content.top + layout.scrolled;
    return std::max<std::int64_t>(from, 0) / std::max<std::int64_t>(layout.line_height, 1);
}
} // namespace nenenib::core
