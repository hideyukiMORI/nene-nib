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
// 「＋」と窓の操作の間の掴む余白・「∨」・× の領域（docs/design/2026-09-29-tabs.md 第 3 節）。
constexpr std::int32_t grab_dips = 40;
constexpr std::int32_t tab_list_dips = 32;
constexpr std::int32_t tab_close_dips = 24;
constexpr std::int32_t tab_close_inset_dips = 6;
// ホイール 1 刻みはタブ 1 本ぶん（幅 120 ＋ 間 2）。
constexpr std::int32_t wheel_step_dips = tab_minimum_dips + tab_gap_dips;

[[nodiscard]] std::int32_t count_of(std::size_t tab_count) noexcept
{
    return static_cast<std::int32_t>(std::max<std::size_t>(tab_count, 1));
}

[[nodiscard]] std::int32_t strip_width(std::int32_t tab_width, std::int32_t gap,
                                       std::int32_t count) noexcept
{
    return tab_width * count + gap * (count - 1);
}

[[nodiscard]] std::int32_t tab_width_for(std::int32_t free_width, std::uint32_t dpi,
                                         std::int32_t count) noexcept
{
    const std::int32_t gap = to_pixels(tab_gap_dips, dpi);
    const std::int32_t share = (free_width - gap * (count - 1)) / count;
    return std::clamp(share, to_pixels(tab_minimum_dips, dpi), to_pixels(tab_maximum_dips, dpi));
}

// タブの領域の右端の上限（x 座標）＝ 帯の幅 − 窓の操作 × 3 − 掴む余白 −「＋」− 間。
[[nodiscard]] std::int32_t strip_limit(std::int32_t width, std::uint32_t dpi) noexcept
{
    return width - to_pixels(caption_button_dips, dpi) * 3 - to_pixels(grab_dips, dpi) -
           to_pixels(add_tab_dips, dpi) - to_pixels(tab_gap_dips, dpi);
}

// 全部のタブが幅 120 で入らなければあふれている（D20）。
[[nodiscard]] bool overflows(std::int32_t free_width, std::uint32_t dpi,
                             std::int32_t count) noexcept
{
    return strip_width(to_pixels(tab_minimum_dips, dpi), to_pixels(tab_gap_dips, dpi), count) >
           free_width;
}

// 送り量を 0 以上・「並びの全長 − 領域の幅」以下に収めた物理画素。
[[nodiscard]] std::int32_t clamped_scroll(std::int32_t scroll_dips, std::uint32_t dpi,
                                          std::int32_t strip, std::int32_t viewport) noexcept
{
    return std::clamp(to_pixels(std::max(scroll_dips, 0), dpi), 0, std::max(strip - viewport, 0));
}

// 物理画素を DIP へ。96 DPI では同じ値（application はこの DPI で呼ぶ）。
[[nodiscard]] std::int32_t to_dips(std::int32_t pixels, std::uint32_t dpi) noexcept
{
    const auto divisor = static_cast<std::int32_t>(std::max<std::uint32_t>(dpi, 1));
    return pixels * static_cast<std::int32_t>(reference_dpi) / divisor;
}

[[nodiscard]] LayoutRect caption_button(std::int32_t right, std::int32_t height,
                                        std::int32_t button) noexcept
{
    return LayoutRect{right - button, 0, right, height};
}

[[nodiscard]] bool hovering(const std::optional<TitleBarTarget> &hovered,
                            std::size_t index) noexcept
{
    if (!hovered.has_value())
    {
        return false;
    }
    const TitleBarTarget target = hovered.value();
    return (target.hit == TitleBarHit::tab || target.hit == TitleBarHit::tab_close) &&
           target.tab == index;
}

// タブの領域の中の点。タブの間と、並びの端より外は caption。
[[nodiscard]] TitleBarTarget tab_target(const TitleBarLayout &layout, std::int32_t x,
                                        std::int32_t y) noexcept
{
    const std::int32_t offset = x - layout.tabs.left;
    const std::int32_t step = layout.tab_width + layout.tab_gap;
    if (offset < 0 || step <= 0 || offset % step >= layout.tab_width)
    {
        return TitleBarTarget{TitleBarHit::caption, 0};
    }
    const auto index = static_cast<std::size_t>(offset / step);
    if (index >= layout.tab_count)
    {
        return TitleBarTarget{TitleBarHit::caption, 0};
    }
    const auto close = tab_close_rect(layout, index);
    if (close.has_value() && contains(close.value(), x, y))
    {
        return TitleBarTarget{TitleBarHit::tab_close, index};
    }
    return TitleBarTarget{TitleBarHit::tab, index};
}

// 窓の操作・「＋」・「∨」。どれにも当たらなければ none を返し、呼び手がタブの領域を見る。
[[nodiscard]] TitleBarHit fixed_part(const TitleBarLayout &layout, std::int32_t x,
                                     std::int32_t y) noexcept
{
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
    if (contains(layout.tab_list, x, y))
    {
        return TitleBarHit::tab_list;
    }
    return TitleBarHit::none;
}
} // namespace

TitleBarLayout title_bar_layout(const TitleBarInput &input) noexcept
{
    const std::uint32_t dpi = input.dpi;
    const std::int32_t height = to_pixels(band_height_dips, dpi);
    const std::int32_t button = to_pixels(caption_button_dips, dpi);
    const std::int32_t left = to_pixels(strip_left_dips, dpi);
    const std::int32_t top = to_pixels(tab_top_dips, dpi);
    const std::int32_t bottom = top + to_pixels(tab_height_dips, dpi);
    const std::int32_t gap = to_pixels(tab_gap_dips, dpi);
    const std::int32_t list = to_pixels(tab_list_dips, dpi);
    const std::int32_t count = count_of(input.tab_count);
    const std::int32_t limit = strip_limit(input.width, dpi);
    const bool overflowing = overflows(std::max(limit - left, 0), dpi, count);
    const std::int32_t tab_width =
        overflowing ? to_pixels(tab_minimum_dips, dpi) : tab_width_for(limit - left, dpi, count);
    const std::int32_t strip = strip_width(tab_width, gap, count);
    // あふれたときはタブの領域の右端がさらに「∨」と間の分だけ左（並びは [領域][∨][＋]）。
    const std::int32_t viewport_right =
        overflowing ? std::max(limit - list - gap, left) : left + strip;
    const std::int32_t scroll =
        overflowing ? clamped_scroll(input.scroll_dips, dpi, strip, viewport_right - left) : 0;
    const std::int32_t list_left = viewport_right + gap;
    const std::int32_t list_right = overflowing ? list_left + list : list_left;
    const std::int32_t add_left = overflowing ? list_right + gap : list_left;
    const LayoutRect close = caption_button(input.width, height, button);
    return TitleBarLayout{
        .band = LayoutRect{0, 0, input.width, height},
        .tabs = LayoutRect{left - scroll, top, left - scroll + strip, bottom},
        .viewport = LayoutRect{left, top, viewport_right, bottom},
        .tab_list = LayoutRect{list_left, top, list_right, bottom},
        .add_tab = LayoutRect{add_left, top, add_left + to_pixels(add_tab_dips, dpi), bottom},
        .minimize = caption_button(close.left - button, height, button),
        .maximize = caption_button(close.left, height, button),
        .close = close,
        .tab_width = tab_width,
        .tab_gap = gap,
        .corner_radius = to_pixels(corner_radius_dips, dpi),
        .underline = to_pixels(underline_dips, dpi),
        .glyph = to_pixels(glyph_dips, dpi),
        .tab_close_size = to_pixels(tab_close_dips, dpi),
        .tab_close_inset = to_pixels(tab_close_inset_dips, dpi),
        .scroll = scroll,
        .overflowing = overflowing,
        .tab_count = static_cast<std::size_t>(count),
        .active = std::min(input.active, static_cast<std::size_t>(count) - 1),
        .hovered = input.hovered};
}

LayoutRect tab_rect(const TitleBarLayout &layout, std::size_t index) noexcept
{
    const auto step = layout.tab_width + layout.tab_gap;
    const std::int32_t left = layout.tabs.left + step * static_cast<std::int32_t>(index);
    return LayoutRect{left, layout.tabs.top, left + layout.tab_width, layout.tabs.bottom};
}

bool tab_visible(const TitleBarLayout &layout, std::size_t index) noexcept
{
    if (index >= layout.tab_count)
    {
        return false;
    }
    const LayoutRect tab = tab_rect(layout, index);
    return tab.right > layout.viewport.left && tab.left < layout.viewport.right;
}

std::optional<LayoutRect> tab_close_rect(const TitleBarLayout &layout, std::size_t index) noexcept
{
    if (index >= layout.tab_count || (index != layout.active && !hovering(layout.hovered, index)))
    {
        return std::nullopt;
    }
    const LayoutRect tab = tab_rect(layout, index);
    const std::int32_t right = tab.right - layout.tab_close_inset;
    const std::int32_t top = tab.top + (height_of(tab) - layout.tab_close_size) / 2;
    return LayoutRect{right - layout.tab_close_size, top, right, top + layout.tab_close_size};
}

TitleBarTarget title_bar_target(const TitleBarLayout &layout, std::int32_t x,
                                std::int32_t y) noexcept
{
    if (!contains(layout.band, x, y))
    {
        return TitleBarTarget{TitleBarHit::none, 0};
    }
    const TitleBarHit part = fixed_part(layout, x, y);
    if (part != TitleBarHit::none)
    {
        return TitleBarTarget{part, 0};
    }
    if (contains(layout.viewport, x, y))
    {
        return tab_target(layout, x, y);
    }
    return TitleBarTarget{TitleBarHit::caption, 0};
}

std::int32_t tabs_scrolled_into_view(const TitleBarInput &input) noexcept
{
    const TitleBarLayout layout = title_bar_layout(input);
    if (!layout.overflowing)
    {
        return 0;
    }
    // 送り量 0 のときのアクティブなタブの左端と右端（タブの領域の左端から測る）。
    const std::int32_t start =
        (layout.tab_width + layout.tab_gap) * static_cast<std::int32_t>(layout.active);
    const std::int32_t end = start + layout.tab_width;
    const std::int32_t visible = width_of(layout.viewport);
    // 右に隠れていれば右端を合わせ、左に隠れていれば左端を合わせる（領域より広いタブは左端）。
    const std::int32_t moved = std::min(std::max(layout.scroll, end - visible), start);
    const std::int32_t limit = std::max(width_of(layout.tabs) - visible, 0);
    return to_dips(std::clamp(moved, 0, limit), input.dpi);
}

std::int32_t tabs_scrolled_by(const TitleBarInput &input, int notches) noexcept
{
    const TitleBarLayout layout = title_bar_layout(input);
    if (!layout.overflowing)
    {
        return 0;
    }
    const std::int64_t moved = static_cast<std::int64_t>(to_dips(layout.scroll, input.dpi)) -
                               (static_cast<std::int64_t>(notches) * wheel_step_dips);
    const std::int32_t limit =
        to_dips(std::max(width_of(layout.tabs) - width_of(layout.viewport), 0), input.dpi);
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(moved, 0, limit));
}
} // namespace nenenib::core
