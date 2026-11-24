// 選ぶ欄の一覧の配置と頭文字の跳び（#1926・採用案 2026-11-24）。選択肢の列そのものは送りの順の表と
// 同じ所（SettingsRows.cpp）にある。
#include "SettingsChoices.hpp"

#include "ContextMenuShape.hpp"
#include "DevicePixels.hpp"

#include <algorithm>

namespace nenenib::core
{
namespace
{
[[nodiscard]] std::int32_t row_height(std::uint32_t dpi) noexcept
{
    return to_pixels(context_menu_row_dips, dpi);
}

[[nodiscard]] std::int32_t inner_gap(std::uint32_t dpi) noexcept
{
    return to_pixels(context_menu_gap_dips, dpi);
}

[[nodiscard]] std::int32_t height_for(std::size_t rows, std::uint32_t dpi) noexcept
{
    return (inner_gap(dpi) * 2) + (row_height(dpi) * static_cast<std::int32_t>(rows));
}

// room の高さに入る行の数（1 以上・wanted 以下）。
[[nodiscard]] std::size_t rows_within(std::int32_t room, std::size_t wanted,
                                      std::uint32_t dpi) noexcept
{
    const auto fitted = std::max((room - (inner_gap(dpi) * 2)) / row_height(dpi), 1);
    return std::min(wanted, static_cast<std::size_t>(fitted));
}

[[nodiscard]] char folded(char byte) noexcept
{
    return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte - 'A' + 'a') : byte;
}

// text が typed で始まるか（ASCII の大小を区別しない）。
[[nodiscard]] bool starts_folded(std::string_view text, std::string_view typed) noexcept
{
    if (typed.size() > text.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < typed.size(); ++index)
    {
        if (folded(text.at(index)) != folded(typed.at(index)))
        {
            return false;
        }
    }
    return true;
}

// 見える行の数（最大 settings_choice_rows）。
[[nodiscard]] std::size_t shown_rows(std::size_t count) noexcept
{
    return std::min(count, settings_choice_rows);
}
} // namespace

std::optional<std::size_t> settings_choice_typed(const std::vector<std::string> &texts,
                                                 std::string_view typed) noexcept
{
    if (typed.empty())
    {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < texts.size(); ++index)
    {
        if (starts_folded(texts.at(index), typed))
        {
            return index;
        }
    }
    return std::nullopt;
}

SettingsChoiceLayout settings_choice_layout(const SettingsChoiceRequest &request) noexcept
{
    const auto dpi = request.dpi;
    const auto gap = inner_gap(dpi);
    const auto &anchor = request.anchor;
    const auto &bounds = request.bounds;
    const auto below = bounds.bottom - (anchor.bottom + gap);
    const auto over = (anchor.top - gap) - bounds.top;
    const std::size_t wanted = shown_rows(request.count);
    const bool fits_below = height_for(wanted, dpi) <= below;
    const bool above = !fits_below && (height_for(wanted, dpi) <= over || over > below);
    const std::size_t rows = wanted == 0 ? 0 : rows_within(above ? over : below, wanted, dpi);
    const auto inside = to_pixels(settings_choice_mark_dips + (settings_choice_padding_dips * 2) +
                                      settings_choice_bar_dips,
                                  dpi);
    const auto width =
        std::min(std::max(width_of(anchor), request.widest + inside), width_of(bounds));
    const auto right = std::min(anchor.right, bounds.right);
    const auto left = std::max(bounds.left, right - width);
    const auto height = height_for(rows, dpi);
    const auto wanted_top = above ? anchor.top - gap - height : anchor.bottom + gap;
    // 1 行ぶんの高さも無い面では、一覧を bounds の中へ寄せる（値の箱に掛かってよい）。
    const auto last_top = std::max(bounds.top, bounds.bottom - height);
    const auto top = std::clamp(wanted_top, bounds.top, last_top);
    const std::size_t first = std::min(request.top, request.count - std::min(rows, request.count));
    return SettingsChoiceLayout{
        LayoutRect{left, top, right, top + height}, first, rows, request.count, above, dpi};
}

LayoutRect settings_choice_row(const SettingsChoiceLayout &layout, std::size_t index) noexcept
{
    const auto top = layout.panel.top + inner_gap(layout.dpi) +
                     (row_height(layout.dpi) * static_cast<std::int32_t>(index));
    return LayoutRect{layout.panel.left, top, layout.panel.right, top + row_height(layout.dpi)};
}

LayoutRect settings_choice_mark(const SettingsChoiceLayout &layout, std::size_t index) noexcept
{
    const auto row = settings_choice_row(layout, index);
    const auto left = row.left + to_pixels(settings_choice_padding_dips, layout.dpi);
    return LayoutRect{left, row.top, left + to_pixels(settings_choice_mark_dips, layout.dpi),
                      row.bottom};
}

LayoutRect settings_choice_label(const SettingsChoiceLayout &layout, std::size_t index) noexcept
{
    const auto row = settings_choice_row(layout, index);
    const auto mark = settings_choice_mark(layout, index);
    const auto right =
        row.right - to_pixels(settings_choice_padding_dips + settings_choice_bar_dips, layout.dpi);
    return LayoutRect{mark.right, row.top, std::max(mark.right, right), row.bottom};
}

LayoutRect settings_choice_thumb(const SettingsChoiceLayout &layout) noexcept
{
    if (layout.count <= layout.rows || layout.rows == 0)
    {
        return LayoutRect{layout.panel.right, layout.panel.top, layout.panel.right,
                          layout.panel.top};
    }
    const auto gap = inner_gap(layout.dpi);
    const auto track_top = layout.panel.top + gap;
    const auto track = height_of(layout.panel) - (gap * 2);
    const auto count = static_cast<std::int64_t>(layout.count);
    const auto rows = static_cast<std::int64_t>(layout.rows);
    const auto least = row_height(layout.dpi) / 2;
    const auto thumb = std::max(static_cast<std::int32_t>(track * rows / count), least);
    const auto travel = static_cast<std::int64_t>(std::max(track - thumb, 0));
    const auto offset = static_cast<std::int32_t>(travel * static_cast<std::int64_t>(layout.first) /
                                                  (count - rows));
    const auto right = layout.panel.right - (to_pixels(settings_choice_bar_dips, layout.dpi) / 2);
    const auto left = right - to_pixels(settings_choice_bar_dips, layout.dpi);
    return LayoutRect{left, track_top + offset, right, track_top + offset + thumb};
}

std::optional<std::size_t> settings_choice_hit(const SettingsChoiceLayout &layout, std::int32_t x,
                                               std::int32_t y) noexcept
{
    const auto top = layout.panel.top + inner_gap(layout.dpi);
    const auto bottom = top + (row_height(layout.dpi) * static_cast<std::int32_t>(layout.rows));
    if (!contains(layout.panel, x, y) || y < top || y >= bottom)
    {
        return std::nullopt;
    }
    return layout.first + static_cast<std::size_t>((y - top) / row_height(layout.dpi));
}

std::size_t settings_choice_revealing(const SettingsChoiceLayout &layout, std::size_t at) noexcept
{
    if (at < layout.first)
    {
        return at;
    }
    if (layout.rows > 0 && at >= layout.first + layout.rows)
    {
        return at + 1 - layout.rows;
    }
    return layout.first;
}

std::size_t settings_choice_centred(std::size_t count, std::size_t at) noexcept
{
    const std::size_t rows = shown_rows(count);
    const std::size_t half = rows / 2;
    const std::size_t top = at >= half ? at - half : 0;
    return std::min(top, count - rows);
}

std::size_t settings_choice_paged(std::size_t count, std::size_t at, bool down) noexcept
{
    if (count == 0)
    {
        return 0;
    }
    const std::size_t step = std::max<std::size_t>(shown_rows(count) - 1, 1);
    return down ? std::min(at + step, count - 1) : (at >= step ? at - step : 0);
}
} // namespace nenenib::core
