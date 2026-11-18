#include "PreviewMetrics.hpp"

#include "DevicePixels.hpp"
#include "LayoutRect.hpp"
#include "Offset.hpp"
#include "PreviewTaskBox.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
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
constexpr float table_padding_x_dips = 14.0F;
constexpr float table_padding_y_dips = 7.0F;
constexpr float table_header_rule_dips = 2.0F;
constexpr float table_rule_dips = 1.0F;
constexpr float quote_rule_dips = 2.0F;
constexpr float alert_rule_dips = 3.0F;
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
    case core::PreviewBlockKind::front_matter:
    case core::PreviewBlockKind::table_row:
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
    case core::PreviewBlockKind::front_matter:
    case core::PreviewBlockKind::table_row:
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
    return preview_paneled(kind) || kind == core::PreviewBlockKind::html;
}

bool preview_paneled(core::PreviewBlockKind kind) noexcept
{
    return kind == core::PreviewBlockKind::code || kind == core::PreviewBlockKind::front_matter;
}

float preview_text_width(const core::PreviewBlock &block, float frame_width, float indent,
                         std::uint32_t dpi) noexcept
{
    const auto padding = preview_paneled(block.kind) ? 2.0F * preview_code_padding(dpi) : 0.0F;
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

float preview_table_padding_x(std::uint32_t dpi) noexcept
{
    return preview_pixels(table_padding_x_dips, dpi);
}

float preview_table_padding_y(std::uint32_t dpi) noexcept
{
    return preview_pixels(table_padding_y_dips, dpi);
}

float preview_table_rule(bool header, std::uint32_t dpi) noexcept
{
    return std::max(preview_pixels(header ? table_header_rule_dips : table_rule_dips, dpi), 1.0F);
}

float preview_table_cell_left(std::span<const float> columns, std::size_t column,
                              std::uint32_t dpi) noexcept
{
    const auto padding = preview_table_padding_x(dpi);
    float left = padding;
    for (std::size_t at = 0; at < column && at < columns.size(); ++at)
    {
        left += columns[at] + 2.0F * padding;
    }
    return left;
}

float preview_table_width(std::span<const float> columns, std::uint32_t dpi) noexcept
{
    return preview_table_cell_left(columns, columns.size(), dpi) - preview_table_padding_x(dpi);
}

float preview_table_aligned(float column_width, float cell_width,
                            core::PreviewTableAlign align) noexcept
{
    const auto room = std::max(column_width - cell_width, 0.0F);
    switch (align)
    {
    case core::PreviewTableAlign::left:
        return 0.0F;
    case core::PreviewTableAlign::center:
        return std::round(room / 2.0F);
    case core::PreviewTableAlign::right:
        return room;
    }
    std::unreachable();
}

float preview_quote_rule(bool alert, std::uint32_t dpi) noexcept
{
    return preview_pixels(alert ? alert_rule_dips : quote_rule_dips, dpi);
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
    // 同じ表の続く行の間は 0（行の間は罫だけ）。
    if (previous.table.has_value() && next.table.has_value() &&
        next.table.value().row == previous.table.value().row + 1U)
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
    case core::PreviewBlockKind::front_matter:
        return text_height + 2.0F * preview_code_padding(dpi);
    case core::PreviewBlockKind::table_row:
        return text_height + 2.0F * preview_table_padding_y(dpi);
    case core::PreviewBlockKind::paragraph:
    case core::PreviewBlockKind::heading:
    case core::PreviewBlockKind::html:
    case core::PreviewBlockKind::list_item:
        return text_height;
    }
    std::unreachable();
}

std::uint32_t preview_units(std::string_view shown, std::size_t byte)
{
    return static_cast<std::uint32_t>(
        core::utf16_length(shown.substr(0, std::min(byte, shown.size()))).value_or(0));
}

namespace
{
// 端ごとに先頭から数えた preview_units の値。
[[nodiscard]] std::vector<std::uint32_t> units_from_start(std::string_view shown,
                                                          std::span<const std::size_t> ends)
{
    std::vector<std::uint32_t> units;
    units.reserve(ends.size());
    std::ranges::transform(ends, std::back_inserter(units),
                           [shown](std::size_t byte) { return preview_units(shown, byte); });
    return units;
}

// 昇順に並べた重複の無い端（文字の長さ以下）ごとの preview_units の値。端の間の
// 切れ端の単位数を前から足す。切れ端が正しい UTF-8 でなければ（端が code point の途中・
// 正しくない文字）、端ごとに先頭から数える（preview_units と同じ値・正しい切れ端だけなら
// 先頭からの数と等しい）。
[[nodiscard]] std::vector<std::uint32_t> units_at(std::string_view shown,
                                                  std::span<const std::size_t> ends)
{
    std::vector<std::uint32_t> units;
    units.reserve(ends.size());
    std::size_t at = 0;
    std::size_t counted = 0;
    for (const auto end : ends)
    {
        const auto piece = core::utf16_length(shown.substr(at, end - at));
        if (!piece)
        {
            return units_from_start(shown, ends);
        }
        counted += piece.value();
        units.push_back(static_cast<std::uint32_t>(counted));
        at = end;
    }
    return units;
}
} // namespace

std::vector<DWRITE_TEXT_RANGE> preview_unit_ranges(std::string_view shown,
                                                   std::span<const PreviewByteRange> ranges)
{
    std::vector<std::size_t> ends;
    ends.reserve(2 * ranges.size());
    for (const auto &[begin, end] : ranges)
    {
        ends.push_back(std::min(begin, shown.size()));
        ends.push_back(std::min(end, shown.size()));
    }
    std::ranges::sort(ends);
    const auto repeated = std::ranges::unique(ends);
    ends.erase(repeated.begin(), repeated.end());
    const auto units = units_at(shown, ends);
    const auto unit_of = [&](std::size_t byte)
    {
        const auto found = std::ranges::lower_bound(ends, std::min(byte, shown.size()));
        return units.at(static_cast<std::size_t>(std::distance(ends.begin(), found)));
    };
    std::vector<DWRITE_TEXT_RANGE> placed;
    placed.reserve(ranges.size());
    for (const auto &[begin, end] : ranges)
    {
        const auto from = unit_of(begin);
        const auto to = unit_of(end);
        placed.push_back(DWRITE_TEXT_RANGE{from, to > from ? to - from : 0U});
    }
    return placed;
}

std::size_t preview_byte_of_units(std::string_view shown, std::uint32_t units)
{
    std::size_t at = 0;
    std::size_t counted = 0;
    while (at < shown.size())
    {
        const auto next = core::next_code_point(shown, core::Offset{at}).value;
        counted += core::utf16_length(shown.substr(at, next - at)).value_or(1);
        if (counted > units)
        {
            return at;
        }
        at = next;
    }
    return shown.size();
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
    return core::PreviewBlock{core::PreviewBlockKind::paragraph,
                              0,
                              0,
                              0,
                              {},
                              item.source_begin,
                              std::string(preview_marker(item)),
                              {},
                              std::nullopt,
                              std::nullopt,
                              std::nullopt,
                              std::nullopt,
                              {}};
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
