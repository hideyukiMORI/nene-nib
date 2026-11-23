#include "HoverTip.hpp"

#include "DevicePixels.hpp"
#include "Offset.hpp"
#include "TabTip.hpp"
#include "VimCharacterBoundary.hpp"

#include <algorithm>
#include <array>

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
    std::size_t size = text.size();
    while (size > 0 && (text[size - 1] == ' ' || text[size - 1] == '\t' || text[size - 1] == '\n'))
    {
        --size;
    }
    return size;
}

// 先頭から lines 行ぶんの長さ（最後の行の改行は含まない）。行が足りなければ値なしの代わりに全体の
// 長さ + 1（越えていない印）。
[[nodiscard]] std::size_t lines_size(std::string_view text, std::size_t lines) noexcept
{
    // 1 文字の検索は libc の memchr を core の外へ出す（ARC-003）ので、バイトを順に数える。
    std::size_t feeds = 0;
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        feeds += text[at] == '\n' ? 1U : 0U;
        if (feeds == lines)
        {
            return at;
        }
    }
    return text.size() + 1;
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
    // 上に入らなければ下。下にもはみ出すなら領域の下端へ寄せ、領域の上端より上へは出さない。
    const std::int32_t below =
        std::max(std::min(caret.bottom + gap, area.bottom - height), area.top);
    const std::int32_t top = above >= area.top ? above : below;
    return LayoutRect{left, top, left + width, top + height};
}

namespace
{
[[nodiscard]] bool overlaps(const LayoutRect &one, const LayoutRect &other) noexcept
{
    return one.left < other.right && other.left < one.right && one.top < other.bottom &&
           other.top < one.bottom;
}
} // namespace

std::optional<LayoutRect> signature_tip_rect(const LayoutRect &caret, const LayoutRect &area,
                                             const TabTipMeasure &text,
                                             const std::optional<LayoutRect> &avoid) noexcept
{
    const LayoutRect placed = hover_tip_rect(caret, area, text);
    if (!avoid.has_value() || !overlaps(placed, avoid.value()))
    {
        return placed;
    }
    const LayoutRect &list = avoid.value();
    const std::int32_t height = height_of(placed);
    const std::int32_t gap = to_pixels(tab_tip_gap_dips, text.dpi);
    // 一覧の中心がキャレットの中心より上なら、一覧は上に回っている。
    const bool list_above = list.top + list.bottom < caret.top + caret.bottom;
    const std::array<std::int32_t, 2> tops{
        list_above ? caret.bottom + gap : caret.top - gap - height,
        list_above ? list.top - gap - height : list.bottom + gap};
    for (const std::int32_t top : tops)
    {
        const LayoutRect box{placed.left, top, placed.right, top + height};
        if (top >= area.top && box.bottom <= area.bottom && !overlaps(box, list) &&
            !overlaps(box, caret))
        {
            return box;
        }
    }
    return std::nullopt;
}
} // namespace nenenib::core
