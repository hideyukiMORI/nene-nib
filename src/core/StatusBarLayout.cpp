#include "StatusBarLayout.hpp"

#include "CommandLayout.hpp"
#include "DevicePixels.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::int32_t band_height_dips = 28;
constexpr std::int32_t side_padding_dips = 12;
constexpr std::int32_t item_gap_dips = 16;
constexpr std::int32_t toggle_padding_dips = 2;
constexpr std::int32_t segment_width_dips = 44;
constexpr std::int32_t segment_height_dips = 20;
constexpr std::int32_t mode_width_dips = 72;
constexpr std::int32_t corner_radius_dips = 6;
constexpr std::int32_t segment_radius_dips = 4;
// 右の 4 項目の幅（行と桁・文字コード・改行・言語）。左から右へ並べた順に対応する。
// 文字コードは「UTF-8 BOM」「Shift_JIS」まで入る幅が要る（ADR 0010 の決定 14 で 44 → 72）。
// 言語は最も長い名前「javascript」「typescript」まで入る幅（ADR 0154 の決定 6）。
constexpr std::array<std::int32_t, status_item_count> item_width_dips{96, 72, 36, 72};
// 診断の件数の幅（ADR 0161 の決定 4）。E と W の和は上限の 1,000 件なので「E500 W500+」まで入る幅。
constexpr std::int32_t diagnostics_width_dips = 96;
// 表示形式の印（ADR 0140 の決定 3）。最初の項目の左 2 DIP から 24 DIP。案内は最初の項目の左
// 28 DIP で終わるので重ならない。
constexpr std::int32_t display_gap_dips = 2;
constexpr std::int32_t display_width_dips = 24;

[[nodiscard]] LayoutRect toggle_box(std::int32_t band_top, std::int32_t band_height,
                                    std::uint32_t dpi) noexcept
{
    const std::int32_t padding = to_pixels(toggle_padding_dips, dpi);
    const std::int32_t width = to_pixels(segment_width_dips, dpi) * 2 + padding * 3;
    const std::int32_t height = to_pixels(segment_height_dips, dpi) + padding * 2;
    const std::int32_t left = to_pixels(side_padding_dips, dpi);
    const std::int32_t top = band_top + (band_height - height) / 2;
    return LayoutRect{left, top, left + width, top + height};
}

// 言語の欄（右端・ADR 0154 の決定 6）の添字。
constexpr std::size_t language_item = status_item_count - 1;
// 文字コードと改行の欄の添字（status_items_for の順・ADR 0010 の追補 #1049 の決定 2 の (a)）。
constexpr std::size_t encoding_item = 1;
constexpr std::size_t line_ending_item = 2;

// 右から並べる。right は右端の項目（言語）の右端。folded なら言語の欄は右端の幅 0 の矩形で、ほかの
// 3 項目は言語が無いときの位置。
[[nodiscard]] std::array<LayoutRect, status_item_count>
item_rects(std::int32_t right, const LayoutRect &band, std::uint32_t dpi, bool folded) noexcept
{
    std::array<LayoutRect, status_item_count> rectangles{};
    for (std::size_t step = 0; step < status_item_count; ++step)
    {
        const std::size_t index = status_item_count - 1 - step;
        if (folded && index == language_item)
        {
            rectangles.at(index) = LayoutRect{right, band.top, right, band.bottom};
            continue;
        }
        const std::int32_t item = to_pixels(item_width_dips.at(index), dpi);
        rectangles.at(index) = LayoutRect{right - item, band.top, right, band.bottom};
        right -= item + to_pixels(item_gap_dips, dpi);
    }
    return rectangles;
}

// `:` の入力欄の右端（最初の項目の左から間の余白）が command_input_minimum_dip に届くか
// （ADR 0154 の決定 6）。言語と件数を畳むかの判断はこの式の 1 つ。
[[nodiscard]] bool reaches(const std::array<LayoutRect, status_item_count> &items,
                           std::uint32_t dpi) noexcept
{
    return items.front().left - to_pixels(item_gap_dips, dpi) >=
           to_pixels(command_input_minimum_dip, dpi);
}

// 右の 4 項目で入力欄が届かなければ言語の欄を畳む（ADR 0154 の決定 6・案内を消す ADR 0079 の
// 決定 9 より先）。
[[nodiscard]] std::array<LayoutRect, status_item_count>
fitted_item_rects(std::int32_t right, const LayoutRect &band, std::uint32_t dpi) noexcept
{
    const auto items = item_rects(right, band, dpi, false);
    return reaches(items, dpi) ? items : item_rects(right, band, dpi, true);
}

// 件数の矩形と右の 4 項目（ADR 0161 の決定 4 の追補・R3-1）。件数はいちばん右で、出さないときは
// 右端 edge の幅 0 の矩形で間の余白も取らない。畳む順は言語 → 件数で、件数を置いて言語を畳んでも
// 入力欄が届かなければ件数も幅 0 にする。
[[nodiscard]] std::pair<LayoutRect, std::array<LayoutRect, status_item_count>>
counted_item_rects(std::int32_t edge, const LayoutRect &band, std::uint32_t dpi,
                   const DiagnosticCounts &counts) noexcept
{
    if (counted(counts))
    {
        const LayoutRect diagnostics{edge - to_pixels(diagnostics_width_dips, dpi), band.top, edge,
                                     band.bottom};
        const auto items =
            fitted_item_rects(diagnostics.left - to_pixels(item_gap_dips, dpi), band, dpi);
        if (reaches(items, dpi))
        {
            return {diagnostics, items};
        }
    }
    return {LayoutRect{edge, band.top, edge, band.bottom}, fitted_item_rects(edge, band, dpi)};
}
} // namespace

StatusBarLayout status_bar_layout(std::int32_t width, std::int32_t height, std::uint32_t dpi,
                                  const DiagnosticCounts &counts) noexcept
{
    const std::int32_t band_height = to_pixels(band_height_dips, dpi);
    const std::int32_t top = std::max(height - band_height, 0);
    const LayoutRect band{0, top, width, height};
    const LayoutRect toggle = toggle_box(top, height - top, dpi);
    const std::int32_t padding = to_pixels(toggle_padding_dips, dpi);
    const std::int32_t segment = to_pixels(segment_width_dips, dpi);
    const LayoutRect ordinary{toggle.left + padding, toggle.top + padding,
                              toggle.left + padding + segment, toggle.bottom - padding};
    const std::int32_t vim_left = ordinary.right + padding;
    const std::int32_t mode_left = toggle.right + to_pixels(item_gap_dips, dpi);
    const auto [diagnostics, items] =
        counted_item_rects(width - to_pixels(side_padding_dips, dpi), band, dpi, counts);
    return StatusBarLayout{
        .band = band,
        .toggle = toggle,
        .toggle_ordinary = ordinary,
        .toggle_vim = LayoutRect{vim_left, ordinary.top, vim_left + segment, ordinary.bottom},
        .mode = LayoutRect{mode_left, band.top, mode_left + to_pixels(mode_width_dips, dpi),
                           band.bottom},
        .items = items,
        .diagnostics = diagnostics,
        .display =
            LayoutRect{items.front().left - to_pixels(display_gap_dips + display_width_dips, dpi),
                       band.top, items.front().left - to_pixels(display_gap_dips, dpi),
                       band.bottom},
        .corner_radius = to_pixels(corner_radius_dips, dpi),
        .segment_radius = to_pixels(segment_radius_dips, dpi)};
}

StatusBarLayout status_bar_layout(std::int32_t width, std::int32_t height,
                                  std::uint32_t dpi) noexcept
{
    return status_bar_layout(width, height, dpi,
                             DiagnosticCounts{0, 0, DiagnosticsCount::complete});
}

StatusBarHit status_bar_hit(const StatusBarLayout &layout, std::int32_t x, std::int32_t y) noexcept
{
    if (contains(layout.toggle_ordinary, x, y))
    {
        return StatusBarHit::toggle_ordinary;
    }
    if (contains(layout.toggle_vim, x, y))
    {
        return StatusBarHit::toggle_vim;
    }
    if (contains(layout.items.at(encoding_item), x, y))
    {
        return StatusBarHit::encoding;
    }
    if (contains(layout.items.at(line_ending_item), x, y))
    {
        return StatusBarHit::line_ending;
    }
    return StatusBarHit::none;
}
} // namespace nenenib::core
