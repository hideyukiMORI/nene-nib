#include "PaletteLayout.hpp"

#include "DevicePixels.hpp"

#include <algorithm>
#include <iterator>
#include <span>
#include <utility>

namespace nenenib::core
{
namespace
{
// 面の寸法（DIP・ADR 0062 / 0189・採用案 docs/design/2026-11-16-finder-look.md）。
constexpr std::int32_t edge_dips = 16;
constexpr std::int32_t narrow_dips = 640;
constexpr std::int32_t wide_dips = 1000;
constexpr std::int32_t list_dips = 400;
constexpr std::int32_t least_pane_dips = 360;
constexpr std::int32_t input_dips = 52;
constexpr std::int32_t footer_dips = 34;
constexpr std::int32_t row_dips = 40;
constexpr std::int32_t heading_dips = 28;
constexpr std::int32_t line_dips = 19;
constexpr std::int32_t body_pad_dips = 6;
constexpr std::int32_t number_dips = 40;
constexpr std::int32_t number_gap_dips = 14;
constexpr std::size_t below_row_limit = 5;
constexpr std::size_t below_least_rows = 3;
constexpr std::int32_t least_lines = 6;

// 面の幅（off と below は 640 DIP・right は 1000 DIP）。窓の幅から左右 16 DIP
// を引いた幅を越えない。
[[nodiscard]] std::int32_t panel_width_of(std::int32_t width, std::uint32_t dpi, bool wide) noexcept
{
    const auto margin = to_pixels(edge_dips, dpi);
    return std::max(std::min(to_pixels(wide ? wide_dips : narrow_dips, dpi), width - margin * 2),
                    0);
}

// 面の上端と、面が使える下端（窓の大きさだけで決まる・今の面と同じ式）。
[[nodiscard]] std::pair<std::int32_t, std::int32_t> vertical_span(std::int32_t height,
                                                                  std::uint32_t dpi) noexcept
{
    const auto margin = to_pixels(edge_dips, dpi);
    const auto body_top = std::min(to_pixels(input_dips, dpi), height);
    const auto bottom = std::max(body_top, height - to_pixels(28, dpi) - margin);
    const auto top = std::min(to_pixels(96, dpi), std::max(body_top, bottom - to_pixels(86, dpi)));
    return {top, bottom};
}

// below で当たりに使える行の数（プレビューの最小の 6 行ぶんを残した残り・上限 5）。
[[nodiscard]] std::size_t below_rows(std::int32_t height, std::uint32_t dpi) noexcept
{
    const auto [top, bottom] = vertical_span(height, dpi);
    const auto spare = bottom - top - to_pixels(input_dips + footer_dips + heading_dips, dpi) -
                       to_pixels(body_pad_dips * 2 + line_dips * least_lines, dpi);
    if (spare <= 0)
    {
        return 0;
    }
    return std::min(static_cast<std::size_t>(spare / std::max(to_pixels(row_dips, dpi), 1)),
                    below_row_limit);
}

[[nodiscard]] bool right_fits(std::int32_t width, std::uint32_t dpi) noexcept
{
    return panel_width_of(width, dpi, true) - to_pixels(list_dips, dpi) >=
           to_pixels(least_pane_dips, dpi);
}

[[nodiscard]] bool below_fits(std::int32_t width, std::int32_t height, std::uint32_t dpi) noexcept
{
    return panel_width_of(width, dpi, false) > 0 && below_rows(height, dpi) >= below_least_rows;
}

// 見出し（上 28 DIP）・本文・切り替えの印（見出しの右端の 28 DIP 四方）を欄 pane から切り出す。
void place_pane(PaletteLayout &layout, const LayoutRect &pane, std::uint32_t dpi) noexcept
{
    const auto heading_bottom = std::min(pane.top + to_pixels(heading_dips, dpi), pane.bottom);
    layout.pane = pane;
    layout.pane_header = LayoutRect{pane.left, pane.top, pane.right, heading_bottom};
    layout.pane_body = LayoutRect{pane.left, heading_bottom, pane.right, pane.bottom};
    const auto toggle_left = std::max(pane.left, pane.right - to_pixels(heading_dips, dpi));
    layout.pane_toggle = LayoutRect{toggle_left, pane.top, pane.right, heading_bottom};
}

// 今の面（off）。1 画素も変えない式のまま。
[[nodiscard]] PaletteLayout plain_layout(std::int32_t width, std::int32_t height, std::uint32_t dpi,
                                         std::size_t choices) noexcept
{
    const auto margin = to_pixels(edge_dips, dpi);
    const auto panel_width = panel_width_of(width, dpi, false);
    const auto left = std::max((width - panel_width) / 2, 0);
    const auto [top, bottom] = vertical_span(height, dpi);
    const auto header_height = std::min(to_pixels(input_dips, dpi), bottom - top);
    const auto footer_height = std::min(to_pixels(footer_dips, dpi), bottom - top - header_height);
    const auto row_height = std::max(to_pixels(row_dips, dpi), 1);
    const auto available =
        static_cast<std::size_t>((bottom - top - header_height - footer_height) / row_height);
    const auto count = std::min({std::max(choices, std::size_t{1}), available, palette_row_limit});
    const auto rows_top = top + header_height;
    const auto rows_bottom = rows_top + static_cast<std::int32_t>(count) * row_height;
    const auto right = left + panel_width;
    const auto input_left = std::min(left + margin, right);
    const auto input_right = std::max(input_left, right - margin);
    return PaletteLayout{
        LayoutRect{left, top, right, rows_bottom + footer_height},
        LayoutRect{input_left, top, input_right, rows_top},
        LayoutRect{left, rows_top, right, rows_bottom},
        LayoutRect{input_left, rows_bottom, input_right, rows_bottom + footer_height},
        row_height,
        count};
}

// 右に並べる形。行の欄は窓に入るだけ（最大 8・候補の数に依らない）で、左 400 DIP が当たりの欄。
[[nodiscard]] PaletteLayout right_layout(std::int32_t width, std::int32_t height,
                                         std::uint32_t dpi) noexcept
{
    auto layout = plain_layout(width, height, dpi, palette_row_limit);
    const auto panel_width = panel_width_of(width, dpi, true);
    const auto left = std::max((width - panel_width) / 2, 0);
    const auto right = left + panel_width;
    const auto margin = to_pixels(edge_dips, dpi);
    const auto input_left = std::min(left + margin, right);
    const auto input_right = std::max(input_left, right - margin);
    const auto split = std::min(left + to_pixels(list_dips, dpi), right);
    const auto rows = layout.rows;
    layout.panel = LayoutRect{left, layout.panel.top, right, layout.panel.bottom};
    layout.input = LayoutRect{input_left, layout.input.top, input_right, layout.input.bottom};
    layout.footer = LayoutRect{input_left, layout.footer.top, input_right, layout.footer.bottom};
    layout.rows = LayoutRect{left, rows.top, split, rows.bottom};
    layout.preview = FinderPreview::right;
    place_pane(layout, LayoutRect{split, rows.top, right, rows.bottom}, dpi);
    return layout;
}

// 下に開く形。当たりは最大 5 行（候補の数に依らない）、その下にプレビュー欄（最大 40 行ぶん）。
[[nodiscard]] PaletteLayout below_layout(std::int32_t width, std::int32_t height,
                                         std::uint32_t dpi) noexcept
{
    auto layout = plain_layout(width, height, dpi, palette_row_limit);
    const auto bottom = vertical_span(height, dpi).second;
    const auto count = below_rows(height, dpi);
    const auto rows_top = layout.rows.top;
    const auto rows_bottom = rows_top + static_cast<std::int32_t>(count) * layout.row_height;
    const auto footer_height = to_pixels(footer_dips, dpi);
    const auto most =
        to_pixels(heading_dips + body_pad_dips * 2, dpi) +
        to_pixels(line_dips, dpi) * static_cast<std::int32_t>(finder_preview_line_limit);
    const auto pane_bottom =
        std::max(rows_bottom, std::min(bottom - footer_height, rows_bottom + most));
    const auto panel = layout.panel;
    layout.rows = LayoutRect{panel.left, rows_top, panel.right, rows_bottom};
    layout.visible_rows = count;
    layout.footer = LayoutRect{layout.footer.left, pane_bottom, layout.footer.right,
                               pane_bottom + footer_height};
    layout.panel = LayoutRect{panel.left, panel.top, panel.right, pane_bottom + footer_height};
    layout.preview = FinderPreview::below;
    place_pane(layout, LayoutRect{panel.left, rows_bottom, panel.right, pane_bottom}, dpi);
    return layout;
}
} // namespace

FinderPreview palette_preview_drawn(FinderPreview placement, std::int32_t width,
                                    std::int32_t height, std::uint32_t dpi) noexcept
{
    switch (placement)
    {
    case FinderPreview::automatic:
    case FinderPreview::right:
        if (right_fits(width, dpi))
        {
            return FinderPreview::right;
        }
        return below_fits(width, height, dpi) ? FinderPreview::below : FinderPreview::off;
    case FinderPreview::below:
        return below_fits(width, height, dpi) ? FinderPreview::below : FinderPreview::off;
    case FinderPreview::off:
        return FinderPreview::off;
    }
    std::unreachable();
}

PaletteLayout palette_layout(std::int32_t width, std::int32_t height, std::uint32_t dpi,
                             PaletteContent content) noexcept
{
    switch (palette_preview_drawn(content.placement, width, height, dpi))
    {
    case FinderPreview::right:
        return right_layout(width, height, dpi);
    case FinderPreview::below:
        return below_layout(width, height, dpi);
    case FinderPreview::automatic:
    case FinderPreview::off:
        break;
    }
    return plain_layout(width, height, dpi, content.choices);
}

std::size_t palette_preview_lines(const PaletteLayout &layout, std::uint32_t dpi) noexcept
{
    const auto room = height_of(layout.pane_body) - to_pixels(body_pad_dips * 2, dpi);
    if (room <= 0 || width_of(layout.pane_body) <= 0)
    {
        return 0;
    }
    return std::min(static_cast<std::size_t>(room / std::max(to_pixels(line_dips, dpi), 1)),
                    finder_preview_line_limit);
}

LayoutRect palette_preview_line(const PaletteLayout &layout, std::uint32_t dpi,
                                std::size_t index) noexcept
{
    const auto &body = layout.pane_body;
    const auto height = to_pixels(line_dips, dpi);
    const auto top = body.top + to_pixels(body_pad_dips, dpi) +
                     static_cast<std::int32_t>(std::min(index, finder_preview_line_limit)) * height;
    return LayoutRect{body.left, std::min(top, body.bottom), body.right,
                      std::min(top + height, body.bottom)};
}

LayoutRect palette_preview_number(const LayoutRect &line, std::uint32_t dpi) noexcept
{
    return LayoutRect{line.left, line.top,
                      std::min(line.left + to_pixels(number_dips, dpi), line.right), line.bottom};
}

LayoutRect palette_preview_text(const LayoutRect &line, std::uint32_t dpi) noexcept
{
    const auto left =
        std::min(line.left + to_pixels(number_dips + number_gap_dips, dpi), line.right);
    return LayoutRect{left, line.top, line.right, line.bottom};
}

LayoutRect palette_preview_heading(const PaletteLayout &layout, std::uint32_t dpi) noexcept
{
    const auto &header = layout.pane_header;
    const auto left =
        std::min(header.left + to_pixels(number_gap_dips, dpi), layout.pane_toggle.left);
    return LayoutRect{left, header.top, std::max(left, layout.pane_toggle.left), header.bottom};
}

bool palette_toggle_hit(const PaletteLayout &layout, std::int32_t x, std::int32_t y) noexcept
{
    return width_of(layout.pane_toggle) > 0 && contains(layout.pane_toggle, x, y);
}

LayoutRect palette_row(const PaletteLayout &layout, std::size_t visible_index) noexcept
{
    const auto at = std::min(visible_index, layout.visible_rows);
    const auto top = layout.rows.top + static_cast<std::int32_t>(at) * layout.row_height;
    return LayoutRect{layout.rows.left, top, layout.rows.right,
                      std::min(top + layout.row_height, layout.rows.bottom)};
}

std::optional<std::size_t> palette_hit(const PaletteLayout &layout, std::int32_t x,
                                       std::int32_t y) noexcept
{
    if (!contains(layout.rows, x, y))
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>((y - layout.rows.top) / layout.row_height);
}

std::size_t palette_window_rows(std::int32_t width, std::int32_t height, std::uint32_t dpi,
                                FinderPreview placement) noexcept
{
    return palette_layout(width, height, dpi, PaletteContent{palette_row_limit, placement})
        .visible_rows;
}

namespace
{
// 行の内側。選択の面の内側 8 DIP のさらに 8 DIP 内（今までの描画と 1 画素も変えないため 8 DIP を
// 2 回足す）。
LayoutRect row_inside(const LayoutRect &row, std::uint32_t dpi) noexcept
{
    const auto inset = to_pixels(8, dpi) * 2;
    const auto left = std::min(row.left + inset, row.right);
    return LayoutRect{left, row.top, std::max(left, row.right - inset), row.bottom};
}
} // namespace

LayoutRect palette_row_note(const LayoutRect &row, std::uint32_t dpi) noexcept
{
    return palette_row_note(row, dpi, 0);
}

LayoutRect palette_row_note(const LayoutRect &row, std::uint32_t dpi, std::int32_t content) noexcept
{
    const auto inside = row_inside(row, dpi);
    // 112 DIP 以下の中身では今までと同じ式（丸めも同じ）のまま。広げた分だけ要る幅を足す。
    const auto least = to_pixels(112, dpi);
    const auto width = std::max(least, content);
    if (width_of(inside) < to_pixels(112 + 12 + 120, dpi) + (width - least))
    {
        return LayoutRect{inside.right, inside.top, inside.right, inside.bottom};
    }
    return LayoutRect{inside.right - width, inside.top, inside.right, inside.bottom};
}

LayoutRect palette_row_label(const LayoutRect &row, std::uint32_t dpi, bool noted) noexcept
{
    if (!noted)
    {
        return row_inside(row, dpi);
    }
    return palette_row_label(row, dpi, palette_row_note(row, dpi));
}

LayoutRect palette_row_label(const LayoutRect &row, std::uint32_t dpi,
                             const LayoutRect &note) noexcept
{
    const auto inside = row_inside(row, dpi);
    if (width_of(note) <= 0)
    {
        return inside;
    }
    return LayoutRect{inside.left, inside.top, note.left - to_pixels(12, dpi), inside.bottom};
}

LayoutRect palette_hit_line(const LayoutRect &row, std::uint32_t dpi) noexcept
{
    const auto inside = row_inside(row, dpi);
    const auto top = std::min(row.top + to_pixels(3, dpi), row.bottom);
    return LayoutRect{inside.left, top, inside.right,
                      std::min(top + to_pixels(19, dpi), row.bottom)};
}

LayoutRect palette_hit_place(const LayoutRect &row, std::uint32_t dpi) noexcept
{
    const auto inside = row_inside(row, dpi);
    const auto top = palette_hit_line(row, dpi).bottom;
    return LayoutRect{inside.left, top, inside.right,
                      std::min(top + to_pixels(15, dpi), row.bottom)};
}

LayoutRect palette_progress_place(const PaletteLayout &layout, std::uint32_t dpi,
                                  std::int32_t width, std::int32_t typed) noexcept
{
    const auto &input = layout.input;
    const auto left = input.right - width;
    if (width <= 0 || input.left + typed + to_pixels(palette_hint_gap_dips, dpi) > left)
    {
        return LayoutRect{input.right, input.top, input.right, input.bottom};
    }
    return LayoutRect{left, input.top, input.right, input.bottom};
}

PaletteHintPlace palette_input_hint(const PaletteLayout &layout, std::uint32_t dpi,
                                    std::span<const std::int32_t> widths) noexcept
{
    const auto &input = layout.input;
    const auto room = width_of(input) - to_pixels(palette_hint_gap_dips, dpi);
    const auto fitting = std::ranges::find_if(widths, [room](std::int32_t width)
                                              { return width > 0 && width <= room; });
    const auto first = static_cast<std::size_t>(std::ranges::distance(widths.begin(), fitting));
    const auto width = fitting == widths.end() ? 0 : *fitting;
    return PaletteHintPlace{LayoutRect{input.right - width, input.top, input.right, input.bottom},
                            first};
}
} // namespace nenenib::core
