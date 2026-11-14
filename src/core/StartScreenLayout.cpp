#include "StartScreenLayout.hpp"

#include "DevicePixels.hpp"
#include "StartEntryKind.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace nenenib::core
{
namespace
{
// 鍵の枠の列の最小（本文の案内と同じ 64 DIP・#1122）。
constexpr std::int32_t start_key_minimum_dips = 64;

// 行の寸法（物理画素）。左右の余白・印の升・印と項目の間・行の高さ・鍵の枠と説明の間。
using RowMetrics = std::array<std::int32_t, 5>;
constexpr std::size_t padding_at = 0;
constexpr std::size_t mark_at = 1;
constexpr std::size_t gap_at = 2;
constexpr std::size_t height_at = 3;
constexpr std::size_t key_gap_at = 4;

[[nodiscard]] RowMetrics metrics_of(std::int32_t line_height, std::uint32_t dpi) noexcept
{
    return {to_pixels(start_row_padding_dips, dpi), to_pixels(start_mark_dips, dpi),
            to_pixels(start_mark_gap_dips, dpi), line_height, to_pixels(start_key_gap_dips, dpi)};
}

// 列の中の 1 行（左右の余白・印の升・項目の欄）。
[[nodiscard]] StartRowLayout row_at(const LayoutRect &column, std::int32_t top,
                                    const RowMetrics &metrics) noexcept
{
    const LayoutRect line{column.left, top, column.right, top + metrics.at(height_at)};
    const auto mark_left = std::min(line.left + metrics.at(padding_at), line.right);
    const auto mark_right = std::min(mark_left + metrics.at(mark_at), line.right);
    const auto text_left = std::min(mark_right + metrics.at(gap_at), line.right);
    const auto text_right = std::max(text_left, line.right - metrics.at(padding_at));
    return StartRowLayout{line, LayoutRect{mark_left, top, mark_right, line.bottom},
                          LayoutRect{text_left, top, text_right, line.bottom}};
}

// 見出し（行の左右の余白の内側・高さ 20 DIP）。
[[nodiscard]] LayoutRect heading_at(const LayoutRect &column, std::int32_t top,
                                    std::uint32_t dpi) noexcept
{
    return LayoutRect{column.left + to_pixels(start_row_padding_dips, dpi), top,
                      column.right - to_pixels(start_row_padding_dips, dpi),
                      top + to_pixels(start_heading_dips, dpi)};
}

[[nodiscard]] std::int32_t after_heading(const LayoutRect &heading, std::uint32_t dpi) noexcept
{
    return heading.bottom + to_pixels(start_heading_gap_dips, dpi);
}

// 選べる行を count 行 top から置き、最後の行の下端を返す。
[[nodiscard]] std::int32_t place_rows(StartScreenLayout &layout, std::int32_t top,
                                      std::size_t count, const RowMetrics &metrics)
{
    for (std::size_t index = 0; index < count; ++index)
    {
        layout.rows.push_back(row_at(layout.column, top, metrics));
        top += metrics.at(height_at);
    }
    return top;
}

// 鍵の案内の 3 行。鍵の枠の列はほかの行の項目と同じ左端から key_width、説明はその右から
// 12 DIP の後。
void place_keys(StartScreenLayout &layout, std::int32_t top, const RowMetrics &metrics,
                std::int32_t key_width) noexcept
{
    for (auto &key : layout.keys)
    {
        const auto row = row_at(layout.column, top, metrics);
        const auto key_right = std::min(row.label.left + key_width, row.label.right);
        const auto label_left = std::min(key_right + metrics.at(key_gap_at), row.label.right);
        key = GuideRowLayout{LayoutRect{row.label.left, top, key_right, row.line.bottom},
                             LayoutRect{label_left, top, row.label.right, row.line.bottom}};
        top += metrics.at(height_at);
    }
}

void move_down(LayoutRect &area, std::int32_t delta) noexcept
{
    area.top += delta;
    area.bottom += delta;
}

void move_down(std::optional<LayoutRect> &area, std::int32_t delta) noexcept
{
    if (area.has_value())
    {
        move_down(area.value(), delta);
    }
}

// 上寄せで置いた画面を、本文の面の上下の中央へ動かす（追補 26・#1480）。上の余白は
// max(36 DIP, (面の高さ − ブロックの高さ) / 2)。ブロックが面より高ければ動かさない（上寄せのまま
// 下が切れる）。整数の割り算だけ。
void centered(StartScreenLayout &layout, const LayoutRect &content, std::uint32_t dpi) noexcept
{
    const auto least = to_pixels(start_top_dips, dpi);
    const auto block = height_of(layout.column);
    const auto delta = std::max(least, (height_of(content) - block) / 2) - least;
    if (delta <= 0)
    {
        return;
    }
    move_down(layout.column, delta);
    move_down(layout.title, delta);
    move_down(layout.recent_heading, delta);
    move_down(layout.bookmark_heading, delta);
    move_down(layout.empty_note, delta);
    move_down(layout.start_heading, delta);
    move_down(layout.key_heading, delta);
    for (auto &row : layout.rows)
    {
        move_down(row.line, delta);
        move_down(row.mark, delta);
        move_down(row.label, delta);
    }
    for (auto &key : layout.keys)
    {
        move_down(key.key, delta);
        move_down(key.label, delta);
    }
    if (layout.signature.has_value())
    {
        for (auto &line : layout.signature.value())
        {
            move_down(line, delta);
        }
    }
}

// 鍵の案内の下の署名の 2 行（D90）。上の 36 DIP と署名を含めたブロックが本文の
// 欄の高さに入るときだけ置き、列の下端を署名の下端へ伸ばす。入らなければ置かない
// （狭い窓で最初に落とす）。
void place_signature(StartScreenLayout &layout, const LayoutRect &content,
                     std::uint32_t dpi) noexcept
{
    const auto first = heading_at(
        layout.column, layout.column.bottom + to_pixels(start_section_gap_dips, dpi), dpi);
    const auto second = heading_at(layout.column, first.bottom, dpi);
    const auto block = second.bottom - layout.column.top;
    if (to_pixels(start_top_dips, dpi) + block > height_of(content))
    {
        return;
    }
    layout.signature = std::array<LayoutRect, 2>{first, second};
    layout.column.bottom = second.bottom;
}
} // namespace

StartScreenShape start_screen_shape(std::span<const StartEntry> entries, bool lists_read) noexcept
{
    StartScreenShape shape{};
    for (const auto &entry : entries)
    {
        shape.recent += entry.kind == StartEntryKind::recent ? 1U : 0U;
        shape.bookmarks += entry.kind == StartEntryKind::bookmark ? 1U : 0U;
    }
    shape.empty_note = lists_read && shape.recent == 0 && shape.bookmarks == 0;
    return shape;
}

std::optional<StartScreenLayout> start_screen_layout(const BodyLayout &body, std::uint32_t dpi,
                                                     StartScreenShape shape, std::int32_t key_width)
{
    const auto room = width_of(body.content) - (2 * to_pixels(start_column_margin_dips, dpi));
    const auto width = std::min(to_pixels(start_column_dips, dpi), room);
    const auto height = body.line_height;
    if (width <= 0 || height <= 0)
    {
        return std::nullopt;
    }
    const auto metrics = metrics_of(height, dpi);
    StartScreenLayout layout{};
    const auto left = body.content.left + ((width_of(body.content) - width) / 2);
    const auto top = body.content.top + to_pixels(start_top_dips, dpi);
    layout.column = LayoutRect{left, top, left + width, top};
    layout.title = LayoutRect{left + metrics.at(padding_at), top,
                              left + width - metrics.at(padding_at), top + ((height * 3) / 2)};
    auto next = layout.title.bottom + to_pixels(start_title_gap_dips, dpi);
    const auto gap = to_pixels(start_section_gap_dips, dpi);
    if (shape.recent > 0)
    {
        layout.recent_heading = heading_at(layout.column, next, dpi);
        next = place_rows(layout, after_heading(layout.recent_heading.value(), dpi), shape.recent,
                          metrics) +
               gap;
    }
    if (shape.bookmarks > 0)
    {
        layout.bookmark_heading = heading_at(layout.column, next, dpi);
        next = place_rows(layout, after_heading(layout.bookmark_heading.value(), dpi),
                          shape.bookmarks, metrics) +
               gap;
    }
    if (shape.empty_note)
    {
        auto note = heading_at(layout.column, next, dpi);
        note.bottom = next + height;
        layout.empty_note = note;
        next += height + gap;
    }
    layout.start_heading = heading_at(layout.column, next, dpi);
    next = place_rows(layout, after_heading(layout.start_heading, dpi), 3, metrics) + gap;
    layout.key_heading = heading_at(layout.column, next, dpi);
    const auto keys_top = after_heading(layout.key_heading, dpi);
    place_keys(layout, keys_top, metrics,
               std::max(key_width, to_pixels(start_key_minimum_dips, dpi)));
    layout.column.bottom = keys_top + (3 * height);
    place_signature(layout, body.content, dpi);
    centered(layout, body.content, dpi);
    return layout;
}

LayoutRect start_version_area(const LayoutRect &title, std::int32_t title_width,
                              std::uint32_t dpi) noexcept
{
    const auto left = title.left + std::max(title_width, 0) + to_pixels(start_detail_gap_dips, dpi);
    return LayoutRect{std::min(left, title.right), title.top, title.right, title.bottom};
}

StartCredit start_credit(const StartScreenLayout &layout, std::uint32_t dpi,
                         const StartCreditWidths &widths) noexcept
{
    const auto fits = [](std::int32_t width, const LayoutRect &area) noexcept
    { return width > 0 && width <= width_of(area); };
    if (!fits(widths.version, start_version_area(layout.title, widths.title, dpi)))
    {
        return StartCredit::none;
    }
    if (!layout.signature.has_value() || !fits(widths.copyright, layout.signature.value().at(0)) ||
        !fits(widths.homepage, layout.signature.value().at(1)))
    {
        return StartCredit::version;
    }
    return StartCredit::version_and_signature;
}

std::optional<std::size_t> start_screen_row_at(const StartScreenLayout &layout,
                                               const BodyLayout &body, std::int32_t x,
                                               std::int32_t y) noexcept
{
    if (!contains(body.band, x, y))
    {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < layout.rows.size(); ++index)
    {
        if (contains(layout.rows.at(index).line, x, y))
        {
            return index;
        }
    }
    return std::nullopt;
}
} // namespace nenenib::core
