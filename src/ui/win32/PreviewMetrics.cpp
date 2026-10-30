#include "PreviewMetrics.hpp"

#include "DevicePixels.hpp"
#include "LayoutRect.hpp"
#include "PreviewTaskBox.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr float side_margin_dips = 16.0F;
constexpr float top_margin_dips = 12.0F;
constexpr float quote_indent_dips = 12.0F;
constexpr float list_indent_dips = 24.0F;
constexpr float code_padding_dips = 12.0F;
constexpr float marker_gap_dips = 8.0F;
// チェックボックスの一辺の字の大きさに対する比と、押す範囲の左右の余裕（ADR 0155 の追補 23・24）。
constexpr float task_side_ratio = 0.9F;
constexpr float task_margin_dips = 2.0F;
// 記号の layout の幅。記号は数文字なので折り返さない。
constexpr float marker_layout_width = 100000.0F;
constexpr float rule_dips = 1.0F;
constexpr float line_ratio = 1.6F;
constexpr float points_per_inch = 72.0F;
constexpr float first_heading_scale = 1.6F;
constexpr float second_heading_scale = 1.35F;
constexpr float third_heading_scale = 1.15F;
constexpr std::string_view bullet = "•";
constexpr std::string_view second_bullet = "◦";
constexpr std::string_view deeper_bullet = "▪";

// 要素の前の間（見出しと水平線は行高・ほかは 0）。
[[nodiscard]] float before(core::PreviewBlockKind kind, float line) noexcept
{
    switch (kind)
    {
    case core::PreviewBlockKind::heading:
    case core::PreviewBlockKind::rule:
        return line;
    case core::PreviewBlockKind::paragraph:
    case core::PreviewBlockKind::code:
    case core::PreviewBlockKind::html:
    case core::PreviewBlockKind::list_item:
        return 0.0F;
    }
    std::unreachable();
}

// 要素の後の間（水平線は行高・ほかは行高の半分）。
[[nodiscard]] float after(core::PreviewBlockKind kind, float line) noexcept
{
    switch (kind)
    {
    case core::PreviewBlockKind::rule:
        return line;
    case core::PreviewBlockKind::paragraph:
    case core::PreviewBlockKind::heading:
    case core::PreviewBlockKind::code:
    case core::PreviewBlockKind::html:
    case core::PreviewBlockKind::list_item:
        return line / 2.0F;
    }
    std::unreachable();
}
} // namespace

float preview_pixels(float dips, std::uint32_t dpi) noexcept
{
    return dips * static_cast<float>(dpi) / static_cast<float>(core::reference_dpi);
}

float preview_font_pixels(const PreviewTextStyle &style) noexcept
{
    return style.points * static_cast<float>(style.dpi) / points_per_inch;
}

float preview_line_pixels(const PreviewTextStyle &style) noexcept
{
    const auto dips = style.points * static_cast<float>(core::reference_dpi) / points_per_inch;
    const auto line_dips = static_cast<std::int32_t>(dips * line_ratio + 0.5F);
    return static_cast<float>(std::max(core::to_pixels(line_dips, style.dpi), 1));
}

float preview_heading_scale(std::uint8_t level) noexcept
{
    switch (level)
    {
    case 1:
        return first_heading_scale;
    case 2:
        return second_heading_scale;
    case 3:
        return third_heading_scale;
    default:
        return 1.0F;
    }
}

bool preview_monospaced(core::PreviewBlockKind kind) noexcept
{
    return kind == core::PreviewBlockKind::code || kind == core::PreviewBlockKind::html;
}

float preview_text_width(const core::PreviewBlock &block, float frame_width, float indent,
                         std::uint32_t dpi) noexcept
{
    const auto padding =
        block.kind == core::PreviewBlockKind::code ? 2.0F * preview_code_padding(dpi) : 0.0F;
    const auto width = frame_width - indent - preview_pixels(side_margin_dips, dpi) - padding;
    return std::max(width, 1.0F);
}

float preview_side_margin(std::uint32_t dpi) noexcept
{
    return preview_pixels(side_margin_dips, dpi);
}

float preview_quote_left(std::uint8_t depth, std::uint32_t dpi) noexcept
{
    return preview_pixels(side_margin_dips + quote_indent_dips * static_cast<float>(depth), dpi);
}

float preview_list_indent(std::uint32_t dpi) noexcept
{
    return preview_pixels(list_indent_dips, dpi);
}

float preview_marker_gap(std::uint32_t dpi) noexcept
{
    return preview_pixels(marker_gap_dips, dpi);
}

float preview_code_padding(std::uint32_t dpi) noexcept
{
    return preview_pixels(code_padding_dips, dpi);
}

float preview_top_margin(std::uint32_t dpi) noexcept
{
    return preview_pixels(top_margin_dips, dpi);
}

float preview_gap(const core::PreviewBlock &previous, const core::PreviewBlock &next,
                  const PreviewTextStyle &style, bool same_list) noexcept
{
    if (same_list && previous.kind == core::PreviewBlockKind::list_item &&
        next.kind == core::PreviewBlockKind::list_item)
    {
        return 0.0F;
    }
    const auto line = preview_line_pixels(style);
    return std::max(after(previous.kind, line), before(next.kind, line));
}

float preview_extent(core::PreviewBlockKind kind, float text_height, std::uint32_t dpi) noexcept
{
    switch (kind)
    {
    case core::PreviewBlockKind::rule:
        return std::max(preview_pixels(rule_dips, dpi), 1.0F);
    case core::PreviewBlockKind::code:
        return text_height + 2.0F * preview_code_padding(dpi);
    case core::PreviewBlockKind::paragraph:
    case core::PreviewBlockKind::heading:
    case core::PreviewBlockKind::html:
    case core::PreviewBlockKind::list_item:
        return text_height;
    }
    std::unreachable();
}

std::string_view preview_shown_text(const core::PreviewBlock &block) noexcept
{
    std::string_view text = block.text;
    if (preview_monospaced(block.kind) && text.ends_with('\n'))
    {
        text.remove_suffix(1);
    }
    return text;
}

std::uint32_t preview_units(std::string_view shown, std::size_t byte)
{
    return static_cast<std::uint32_t>(
        core::utf16_length(shown.substr(0, std::min(byte, shown.size()))).value_or(0));
}

std::string_view preview_marker(const core::PreviewBlock &block) noexcept
{
    if (block.marker != bullet)
    {
        return block.marker;
    }
    if (block.list_depth <= 1)
    {
        return bullet;
    }
    return block.list_depth == 2 ? second_bullet : deeper_bullet;
}
core::PreviewBlock preview_marker_block(const core::PreviewBlock &item)
{
    return core::PreviewBlock{
        core::PreviewBlockKind::paragraph, 0,  0,           0, {}, item.source_begin,
        std::string(preview_marker(item)), {}, std::nullopt};
}

float preview_marker_layout_width() noexcept
{
    return marker_layout_width;
}

float preview_task_side(const PreviewTextStyle &style) noexcept
{
    return static_cast<float>(
        static_cast<std::int32_t>(preview_font_pixels(style) * task_side_ratio + 0.5F));
}

PreviewTaskBox preview_task_box(float indent, float top, const PreviewTextStyle &style) noexcept
{
    const auto side = static_cast<std::int32_t>(preview_task_side(style));
    const auto line = static_cast<std::int32_t>(preview_line_pixels(style) + 0.5F);
    const auto right = static_cast<std::int32_t>(indent - preview_marker_gap(style.dpi) + 0.5F);
    const auto row = static_cast<std::int32_t>(top + 0.5F);
    const core::LayoutRect square{right - side, row + (line - side) / 2, right,
                                  row + (line - side) / 2 + side};
    // 押す範囲は 1 行の高さの正方形を正方形と中心をそろえて置き、左右に余裕を足す。
    const auto margin =
        static_cast<std::int32_t>(preview_pixels(task_margin_dips, style.dpi) + 0.5F);
    const auto left = square.left - (line - side) / 2 - margin;
    return PreviewTaskBox{square,
                          core::LayoutRect{left, row, left + line + 2 * margin, row + line}};
}
} // namespace nenenib::ui::win32
