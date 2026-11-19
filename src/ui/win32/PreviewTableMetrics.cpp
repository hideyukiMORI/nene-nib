#include "PreviewTableMetrics.hpp"

#include "PreviewMetrics.hpp"
#include "PreviewTableScroll.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nenenib::ui::win32
{
namespace
{
constexpr float bar_height_dips = 6.0F;
constexpr float bar_gap_dips = 4.0F;

// 物理画素 → DIP（小数のまま）。
[[nodiscard]] float dips_of(float pixels, std::uint32_t dpi) noexcept
{
    return dpi == 0 ? 0.0F : pixels / preview_pixels(1.0F, dpi);
}
} // namespace

PreviewTableBand preview_table_band(std::span<const float> columns, float frame_width, float indent,
                                    std::uint32_t dpi) noexcept
{
    return PreviewTableBand{std::max(frame_width - indent - preview_side_margin(dpi), 0.0F),
                            preview_table_width(columns, dpi)};
}

core::PreviewTableSpan preview_table_span(PreviewTableBand band, std::uint32_t dpi) noexcept
{
    const auto view = static_cast<std::int32_t>(std::floor(dips_of(band.view, dpi)));
    const auto table = static_cast<std::int32_t>(std::ceil(dips_of(band.width, dpi)));
    // 広いかは物理画素で決める（preview_table_overflows）。画素で広い表は DIP でも 1 以上広く、
    // 画素で収まる表は DIP でも広くしない（丸めで棒を出さない・#1884 のレビュー M1）。
    if (!preview_table_overflows(band, dpi))
    {
        return core::PreviewTableSpan{view, std::min(table, view)};
    }
    return core::PreviewTableSpan{view, std::max(table, view + 1)};
}

bool preview_table_overflows(PreviewTableBand band, std::uint32_t dpi) noexcept
{
    // 1 DIP に満たない枠は送れない（DIP の枠が 0）ので、棒も出さない（#1884 の再確認 L-c）。
    return dpi > 0 && band.view >= preview_pixels(1.0F, dpi) && band.width > band.view;
}

std::int32_t preview_table_offset(std::span<const std::int32_t> scrolls, std::size_t table) noexcept
{
    return table < scrolls.size() ? scrolls[table] : 0;
}

float preview_table_shift(PreviewTableBand band, std::int32_t offset, std::uint32_t dpi) noexcept
{
    if (!preview_table_overflows(band, dpi))
    {
        return 0.0F;
    }
    const auto span = preview_table_span(band, dpi);
    const auto clamped = core::preview_table_scroll_clamped(offset, span);
    // DIP の端（表は切り上げ）は画素の端を越え得るので、画素の端でも止める。
    return std::clamp(preview_pixels(static_cast<float>(clamped), dpi), 0.0F,
                      std::max(band.width - band.view, 0.0F));
}

float preview_table_track_pixels(PreviewTableBand band, std::int32_t dips,
                                 std::uint32_t dpi) noexcept
{
    const auto span = preview_table_span(band, dpi);
    return span.view <= 0 ? 0.0F
                          : static_cast<float>(dips) * band.view / static_cast<float>(span.view);
}

std::int32_t preview_table_track_dips(PreviewTableBand band, float pixels,
                                      std::uint32_t dpi) noexcept
{
    const auto span = preview_table_span(band, dpi);
    return band.view <= 0.0F ? 0
                             : static_cast<std::int32_t>(
                                   std::floor(pixels * static_cast<float>(span.view) / band.view));
}

bool preview_table_closes(const core::MarkdownDocument &document, std::size_t index) noexcept
{
    if (index >= document.blocks.size())
    {
        return false;
    }
    const auto &row = document.blocks.at(index).table;
    if (!row.has_value())
    {
        return false;
    }
    if (index + 1U >= document.blocks.size())
    {
        return true;
    }
    const auto &next = document.blocks.at(index + 1U).table;
    return !next.has_value() || next.value().table != row.value().table;
}

float preview_table_bar_height(std::uint32_t dpi) noexcept
{
    return preview_pixels(bar_height_dips, dpi);
}

float preview_table_bar_gap(std::uint32_t dpi) noexcept
{
    return preview_pixels(bar_gap_dips, dpi);
}

float preview_table_bar_extent(std::uint32_t dpi) noexcept
{
    return preview_table_bar_gap(dpi) + preview_table_bar_height(dpi);
}
} // namespace nenenib::ui::win32
