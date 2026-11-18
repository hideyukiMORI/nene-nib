#include "HoverTip.hpp"

#include "DevicePixels.hpp"
#include "Offset.hpp"
#include "TabTip.hpp"
#include "VimCharacterBoundary.hpp"

#include <algorithm>

namespace nenenib::core
{
namespace
{
constexpr std::string_view clipped_mark = "…";

// 改行を LF にした文（CRLF と単独の CR は LF 1 つ）。
[[nodiscard]] std::string line_feeds(std::string_view utf8)
{
    std::string text;
    text.reserve(utf8.size());
    for (std::size_t at = 0; at < utf8.size(); ++at)
    {
        if (utf8[at] != '\r')
        {
            text.push_back(utf8[at]);
            continue;
        }
        text.push_back('\n');
        at += (at + 1 < utf8.size() && utf8[at + 1] == '\n') ? 1U : 0U;
    }
    return text;
}

// 末尾の空白と改行を除いた長さ。
[[nodiscard]] std::size_t trimmed_size(std::string_view text) noexcept
{
    const std::size_t last = text.find_last_not_of(" \t\n");
    return last == std::string_view::npos ? 0 : last + 1;
}

// 先頭から lines 行ぶんの長さ（最後の行の改行は含まない）。行が足りなければ値なしの代わりに全体の
// 長さ + 1（越えていない印）。
[[nodiscard]] std::size_t lines_size(std::string_view text, std::size_t lines) noexcept
{
    std::size_t at = 0;
    for (std::size_t line = 0; line < lines; ++line)
    {
        const std::size_t feed = text.find('\n', at);
        if (feed == std::string_view::npos)
        {
            return text.size() + 1;
        }
        if (line + 1 == lines)
        {
            return feed;
        }
        at = feed + 1;
    }
    return 0;
}

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

// 末尾の空白と改行を落として「…」を付けた文。
[[nodiscard]] std::string marked(std::string_view kept)
{
    std::string text(kept.substr(0, trimmed_size(kept)));
    text += clipped_mark;
    return text;
}
} // namespace

std::string hover_tip_text(std::string_view utf8)
{
    std::string text = line_feeds(utf8);
    text.resize(trimmed_size(text));
    const std::size_t kept = lines_size(text, hover_tip_max_lines);
    return kept > text.size() ? text : marked(std::string_view{text}.substr(0, kept));
}

std::string hover_tip_clipped(std::string_view utf8, std::size_t kept)
{
    const std::size_t end = after_characters(utf8, kept);
    return end >= utf8.size() ? std::string(utf8) : marked(utf8.substr(0, end));
}

std::int32_t hover_tip_text_limit(std::int32_t area_width, std::uint32_t dpi) noexcept
{
    const std::int32_t box = std::min(to_pixels(hover_tip_max_width_dips, dpi),
                                      area_width - (2 * to_pixels(tab_tip_edge_dips, dpi)));
    return std::max(box - (2 * to_pixels(tab_tip_padding_x_dips, dpi)), 0);
}

LayoutRect hover_tip_rect(const LayoutRect &caret, const LayoutRect &area,
                          const TabTipMeasure &text) noexcept
{
    const std::int32_t width = text.width + (2 * to_pixels(tab_tip_padding_x_dips, text.dpi));
    const std::int32_t height = text.height + (2 * to_pixels(tab_tip_padding_y_dips, text.dpi));
    const std::int32_t gap = to_pixels(tab_tip_gap_dips, text.dpi);
    const std::int32_t right_most = area.right - to_pixels(tab_tip_edge_dips, text.dpi) - width;
    const std::int32_t left = std::max(std::min(caret.left, right_most), area.left);
    const std::int32_t above = caret.top - gap - height;
    const std::int32_t top = above >= area.top ? above : caret.bottom + gap;
    return LayoutRect{left, top, left + width, top + height};
}
} // namespace nenenib::core
