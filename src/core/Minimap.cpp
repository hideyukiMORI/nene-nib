#include "Minimap.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] bool blank(char32_t value) noexcept
{
    return value == U' ' || value == U'\t' || value == U'　';
}

// 序数 ordinal（1 始まり）の文字の役割。span は区間の列の読みかけの位置で、前へだけ進む。
[[nodiscard]] std::optional<SyntaxRole> role_at(std::span<const ColorSpan> colors,
                                                std::size_t &span, std::size_t ordinal) noexcept
{
    while (span < colors.size() && colors[span].end.value <= ordinal)
    {
        ++span;
    }
    if (span < colors.size() && colors[span].begin.value <= ordinal)
    {
        return colors[span].role;
    }
    return std::nullopt;
}

void append_block(MinimapRow &row, std::size_t column, std::size_t width,
                  std::optional<SyntaxRole> role)
{
    if (!row.empty() && row.back().role == role &&
        static_cast<std::size_t>(row.back().left) + row.back().width == column)
    {
        row.back().width = static_cast<std::uint16_t>(row.back().width + width);
        return;
    }
    row.push_back(
        MinimapBlock{static_cast<std::uint16_t>(column), static_cast<std::uint16_t>(width), role});
}
} // namespace

bool minimap_shown(const EditorSettings &settings) noexcept
{
    return std::ranges::find(settings.features, minimap_feature) != settings.features.end();
}

EditorSettings minimap_set(EditorSettings settings, bool shown)
{
    auto &features = settings.features;
    std::erase(features, minimap_feature);
    if (shown)
    {
        features.emplace_back(minimap_feature);
    }
    return settings;
}

MinimapRow minimap_row(std::string_view line, std::span<const ColorSpan> colors)
{
    MinimapRow row;
    std::size_t column = 0;
    std::size_t ordinal = 1;
    std::size_t span = 0;
    Offset at{0};
    while (at.value < line.size() && column < minimap_columns)
    {
        const char32_t value = code_point_at(line, at);
        const std::size_t advance = column_advance(value, column);
        const auto role = role_at(colors, span, ordinal);
        if (!blank(value) && advance > 0)
        {
            append_block(row, column, std::min(advance, minimap_columns - column), role);
        }
        column += advance;
        ++ordinal;
        at = next_code_point(line, at);
    }
    return row;
}

LineNumber minimap_top(LineNumber first_visible, std::size_t visible, std::size_t total,
                       std::size_t rows) noexcept
{
    if (total <= rows || total <= visible)
    {
        return LineNumber{1};
    }
    const std::size_t first = std::clamp(first_visible.value, std::size_t{1}, total) - 1;
    const std::size_t scrollable = total - rows;
    const std::size_t top = std::min(first * scrollable / (total - visible), scrollable);
    return LineNumber{std::min(top, first) + 1};
}

MinimapWindow minimap_window(const std::optional<MinimapWindow> &held, LineNumber top,
                             std::size_t rows, std::size_t total) noexcept
{
    const std::size_t start =
        std::clamp(top.value, std::size_t{1}, std::max(total, std::size_t{1})) - 1;
    const std::size_t needed = std::min({rows, total - start, minimap_reach * 2});
    if (held.has_value())
    {
        const auto &window = held.value();
        const std::size_t first = window.first.value - 1;
        if (first <= start && start + needed <= first + window.rows &&
            first + window.rows <= total && window.rows <= minimap_window_limit)
        {
            return window;
        }
    }
    const std::size_t centre = start + needed / 2;
    std::size_t first = centre > minimap_reach ? centre - minimap_reach : 0;
    first = std::min(first, total > minimap_reach * 2 ? total - minimap_reach * 2 : 0);
    return MinimapWindow{LineNumber{first + 1}, std::min(minimap_reach * 2, total - first)};
}
} // namespace nenenib::core
