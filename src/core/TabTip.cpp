#include "TabTip.hpp"

#include "DevicePixels.hpp"
#include "Offset.hpp"
#include "TitleBarHit.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimCharacterBoundary.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::string_view middle_mark = "…";

// 先頭から count 文字の後ろのバイト位置。
[[nodiscard]] std::size_t after_characters(std::string_view utf8, std::size_t count) noexcept
{
    Offset at{0};
    for (std::size_t walked = 0; walked < count && at.value < utf8.size(); ++walked)
    {
        at = vim_character_end(utf8, at);
    }
    return at.value;
}

// 末尾から count 文字の先頭のバイト位置。
[[nodiscard]] std::size_t before_characters(std::string_view utf8, std::size_t count) noexcept
{
    Offset at{utf8.size()};
    for (std::size_t walked = 0; walked < count && at.value > 0; ++walked)
    {
        at = vim_character_start(utf8, at);
    }
    return at.value;
}
} // namespace

std::optional<std::size_t> tab_tip_tab(const std::optional<TitleBarTarget> &hovered) noexcept
{
    if (!hovered.has_value())
    {
        return std::nullopt;
    }
    switch (hovered.value().hit)
    {
    case TitleBarHit::tab:
    case TitleBarHit::tab_close:
        return hovered.value().tab;
    case TitleBarHit::add_tab:
    case TitleBarHit::tab_list:
    case TitleBarHit::caption:
    case TitleBarHit::minimize:
    case TitleBarHit::maximize:
    case TitleBarHit::close:
    case TitleBarHit::none:
        return std::nullopt;
    }
    std::unreachable();
}

std::string tab_tip_text(const std::optional<FilePath> &path, DisplayLanguage language)
{
    if (!path.has_value())
    {
        return std::string(ui_text(UiMessage::tab_untitled, language));
    }
    std::string text(path.value().text());
    std::ranges::replace(text, '/', '\\');
    return text;
}

std::size_t tab_tip_characters(std::string_view utf8) noexcept
{
    std::size_t count = 0;
    for (Offset at{0}; at.value < utf8.size(); at = vim_character_end(utf8, at))
    {
        ++count;
    }
    return count;
}

std::string middle_clipped(std::string_view utf8, std::size_t dropped)
{
    if (dropped == 0)
    {
        return std::string(utf8);
    }
    const std::size_t total = tab_tip_characters(utf8);
    const std::size_t kept = total - std::min(dropped, total);
    const std::size_t head = after_characters(utf8, kept / 2);
    const std::size_t tail = before_characters(utf8, kept - (kept / 2));
    std::string clipped(utf8.substr(0, head));
    clipped += middle_mark;
    clipped += utf8.substr(tail);
    return clipped;
}

std::int32_t tab_tip_text_limit(std::int32_t window_width, std::uint32_t dpi) noexcept
{
    const std::int32_t limit = window_width - (2 * to_pixels(tab_tip_edge_dips, dpi)) -
                               (2 * to_pixels(tab_tip_padding_x_dips, dpi));
    return std::max(limit, 0);
}

LayoutRect tab_tip_rect(const TitleBarLayout &layout, std::size_t tab,
                        const TabTipMeasure &text) noexcept
{
    const std::int32_t width = text.width + (2 * to_pixels(tab_tip_padding_x_dips, text.dpi));
    const std::int32_t height = text.height + (2 * to_pixels(tab_tip_padding_y_dips, text.dpi));
    const std::int32_t tab_left = std::max(tab_rect(layout, tab).left, layout.viewport.left);
    const std::int32_t right_most =
        layout.band.right - to_pixels(tab_tip_edge_dips, text.dpi) - width;
    const std::int32_t left = std::max(std::min(tab_left, right_most), 0);
    const std::int32_t top = layout.band.bottom + to_pixels(tab_tip_gap_dips, text.dpi);
    return LayoutRect{left, top, left + width, top + height};
}
} // namespace nenenib::core
