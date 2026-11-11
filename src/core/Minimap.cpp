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

namespace
{
// 整数の輝度の近似（channel² に WCAG の重みの 10,000 倍）。最大は 10,000 × 255² = 650,250,000。
[[nodiscard]] std::int64_t approximate_luminance(RgbColor color) noexcept
{
    const auto square = [](std::uint8_t value)
    { return std::int64_t{value} * std::int64_t{value}; };
    return 2126 * square(color.red) + 7152 * square(color.green) + 722 * square(color.blue);
}
} // namespace

bool minimap_contrast_at_least(RgbColor first, RgbColor second, std::int64_t tenths) noexcept
{
    // (Y + 0.05) の 0.05 は 10,000 × 255² × 0.05 = 32,512,500。
    constexpr std::int64_t offset = 32'512'500;
    const std::int64_t one = approximate_luminance(first) + offset;
    const std::int64_t other = approximate_luminance(second) + offset;
    return std::max(one, other) * 10 >= std::min(one, other) * tenths;
}

RgbColor minimap_face_color(const Palette &palette) noexcept
{
    return minimap_contrast_at_least(palette.accent, palette.background,
                                     minimap_face_contrast_tenths)
               ? palette.accent
               : palette.text;
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

std::int32_t minimap_pointed_row(std::int32_t y, std::int32_t panel_top) noexcept
{
    // 面の上へ出た点も段に直す（負の側は切り下げ）。
    const std::int64_t offset = std::int64_t{y} - std::int64_t{panel_top};
    const std::int64_t row = offset >= 0
                                 ? offset / minimap_row_pixels
                                 : -((-offset + minimap_row_pixels - 1) / minimap_row_pixels);
    return static_cast<std::int32_t>(row);
}

LineNumber minimap_pointed_first(std::int32_t row, std::size_t visible, std::size_t total,
                                 std::size_t rows) noexcept
{
    if (total <= visible)
    {
        return LineNumber{1};
    }
    // 見えている最初の行（0 始まり）の上限。minimap_top の送りの分母と同じ。
    const std::size_t last = total - visible;
    const auto last_signed = static_cast<std::int64_t>(last);
    const auto half = static_cast<std::int64_t>(visible / 2);
    // 文書が面に収まる: 面は送られない（minimap_top は 1 行目）ので、段 row は行 row + 1。
    if (total <= rows)
    {
        const std::int64_t first =
            std::clamp<std::int64_t>(std::int64_t{row} - half, 0, last_signed);
        return LineNumber{static_cast<std::size_t>(first) + 1};
    }
    // 面が見えている行より短い: 面の最上段は見えている最初の行に付いて動くので、段の割合を文書に
    // 写す（今のスクロールに依らない）。
    if (rows <= visible)
    {
        const std::size_t span = rows > 1 ? rows - 1 : 1;
        const auto at = static_cast<std::size_t>(
            std::clamp<std::int64_t>(row, 0, static_cast<std::int64_t>(span)));
        return LineNumber{std::min((at * last + span / 2) / span, last) + 1};
    }
    // 見えている範囲の面（visible 段）の上端が動ける段の数。上端 slider の面は minimap_top の比例の
    // 送りで、見えている最初の行 first のとき first * track / last 段目に来る。その逆で、面の中央が
    // row に来る first を四捨五入で求める。
    const std::size_t track = rows - visible;
    const auto slider = static_cast<std::size_t>(
        std::clamp<std::int64_t>(std::int64_t{row} - half, 0, static_cast<std::int64_t>(track)));
    return LineNumber{std::min((slider * last + track / 2) / track, last) + 1};
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
