#include "SettingsPanel.hpp"

#include "DevicePixels.hpp"
#include "EditorFeature.hpp"
#include "SettingsItemKind.hpp"
#include "SettingsLayoutRow.hpp"
#include "SettingsPart.hpp"
#include "SettingsTextWidths.hpp"
#include "SettingsValue.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
// 採用案の見本の寸法（DIP）。面の左右の余白・見出しの行・探す欄・札・行の領域・下の帯。
constexpr std::int32_t side_dips = 18;
constexpr std::int32_t header_top_dips = 16;
constexpr std::int32_t header_height_dips = 32;
constexpr std::int32_t header_bottom_dips = 10;
constexpr std::int32_t search_height_dips = 36;
constexpr std::int32_t block_gap_dips = 10;
constexpr std::int32_t chip_height_dips = 26;
constexpr std::int32_t chip_padding_dips = 11;
constexpr std::int32_t chip_gap_dips = 6;
constexpr std::int32_t list_top_dips = 8;
constexpr std::int32_t list_bottom_dips = 16;
constexpr std::int32_t list_side_dips = 22;
constexpr std::int32_t row_side_dips = 10;
constexpr std::int32_t heading_height_dips = 25;
constexpr std::int32_t section_bottom_dips = 4;
constexpr std::int32_t summary_top_dips = 2;
constexpr std::int32_t summary_bottom_dips = 6;
constexpr std::int32_t summary_note_gap_dips = 4;
constexpr std::int32_t summary_note_height_dips = 16;
constexpr std::int32_t summary_padding_dips = 11;
constexpr std::int32_t preset_height_dips = 30;
constexpr std::int32_t preset_padding_dips = 12;
constexpr std::int32_t empty_height_dips = 96;
constexpr std::int32_t footer_height_dips = 58;
constexpr std::int32_t footer_padding_dips = 12;
constexpr std::int32_t button_height_dips = 34;
constexpr std::int32_t button_width_dips = 84;
constexpr std::int32_t button_padding_dips = 15;
constexpr std::int32_t button_gap_dips = 8;
constexpr std::int32_t step_width_dips = 28;
constexpr std::int32_t step_height_dips = 26;
constexpr std::int32_t step_inset_dips = 2;

constexpr LayoutRect nowhere{0, 0, 0, 0};

[[nodiscard]] SettingsControl value_control(SettingsValue value) noexcept
{
    switch (value)
    {
    case SettingsValue::theme:
    case SettingsValue::ui_language:
    case SettingsValue::finder_preview:
        return SettingsControl::choice;
    case SettingsValue::font_size:
    case SettingsValue::caret_motion_time:
        return SettingsControl::stepper;
    case SettingsValue::font_family:
        return SettingsControl::readonly;
    }
    std::unreachable();
}

// 面・左の境・見出しの行・✕・探す欄。
[[nodiscard]] SettingsLayout framed(const SettingsLayoutRequest &request)
{
    const auto dpi = request.dpi;
    const auto &area = request.area;
    const bool narrow = to_dips(width_of(area), dpi) < settings_narrow_below_dips;
    const auto left = narrow ? area.left : area.right - to_pixels(settings_panel_width_dips, dpi);
    const auto inner = narrow ? left : left + to_pixels(1, dpi);
    const auto side = to_pixels(side_dips, dpi);
    const auto top = area.top + to_pixels(header_top_dips, dpi);
    const auto bottom = top + to_pixels(header_height_dips, dpi);
    const LayoutRect close{area.right - side - to_pixels(header_height_dips, dpi), top,
                           area.right - side, bottom};
    const auto search_top = bottom + to_pixels(header_bottom_dips, dpi);
    SettingsLayout layout{};
    layout.panel = LayoutRect{left, area.top, area.right, area.bottom};
    layout.border = LayoutRect{left, area.top, inner, area.bottom};
    layout.header =
        LayoutRect{inner + side, top, close.left - to_pixels(block_gap_dips, dpi), bottom};
    layout.close = close;
    layout.search = LayoutRect{inner + side, search_top, area.right - side,
                               search_top + to_pixels(search_height_dips, dpi)};
    layout.narrow = narrow;
    layout.dpi = dpi;
    return layout;
}

// 札を左から並べ、右端を越える札は次の段へ折り返す。最後の段の下端を返す。
[[nodiscard]] std::int32_t place_chips(SettingsLayout &layout, const SettingsTextWidths &widths)
{
    const auto dpi = layout.dpi;
    const auto start = layout.search.left;
    const auto limit = layout.search.right;
    const auto height = to_pixels(chip_height_dips, dpi);
    const auto gap = to_pixels(chip_gap_dips, dpi);
    std::int32_t x = start;
    std::int32_t y = layout.search.bottom + to_pixels(block_gap_dips, dpi);
    for (std::size_t index = 0; index < layout.chips.size(); ++index)
    {
        const auto width = widths.chips.at(index) + (to_pixels(chip_padding_dips, dpi) * 2);
        if (x != start && x + width > limit)
        {
            x = start;
            y += height + gap;
        }
        layout.chips.at(index) = LayoutRect{x, y, x + width, y + height};
        x += width + gap;
    }
    return y + height;
}

// 下の帯（上の線・「閉じると保存されます」・元に戻す・閉じる）。
void place_footer(SettingsLayout &layout, const SettingsTextWidths &widths)
{
    const auto dpi = layout.dpi;
    const auto &panel = layout.panel;
    const auto band = panel.bottom - to_pixels(footer_height_dips, dpi);
    const auto top = band + to_pixels(footer_padding_dips, dpi);
    const auto bottom = top + to_pixels(button_height_dips, dpi);
    const auto width_of_button = [dpi](std::int32_t text)
    {
        return std::max(to_pixels(button_width_dips, dpi),
                        text + (to_pixels(button_padding_dips, dpi) * 2));
    };
    const auto right = panel.right - to_pixels(side_dips, dpi);
    layout.close_button = LayoutRect{right - width_of_button(widths.close), top, right, bottom};
    const auto revert_right = layout.close_button.left - to_pixels(button_gap_dips, dpi);
    layout.revert =
        LayoutRect{revert_right - width_of_button(widths.revert), top, revert_right, bottom};
    layout.footer_line =
        LayoutRect{layout.border.right, band - to_pixels(1, dpi), panel.right, band};
    layout.note = LayoutRect{layout.search.left, top,
                             layout.revert.left - to_pixels(button_gap_dips, dpi), bottom};
}

// 行の領域に 1 つ積む。y は次の上端へ進む。
void push_item(SettingsLayout &layout, std::int32_t &y, SettingsLayoutItem item,
               std::int32_t height_dips)
{
    const auto dpi = layout.dpi;
    const bool row = item.kind == SettingsItemKind::row;
    const auto side = to_pixels(row ? row_side_dips : list_side_dips, dpi);
    const auto height = to_pixels(height_dips, dpi);
    item.box = LayoutRect{layout.list.left + side, y, layout.list.right - side, y + height};
    layout.items.push_back(item);
    y += height;
}

// 要約の箱の高さ（DIP）。1 行目の文 estimate が箱の文の幅に入り切らなければ 1 行ぶん高い。
[[nodiscard]] std::int32_t summary_height(const SettingsLayout &layout,
                                          std::int32_t estimate) noexcept
{
    const auto dpi = layout.dpi;
    const auto room =
        width_of(layout.list) - (to_pixels(list_side_dips + settings_summary_inset_dips, dpi) * 2);
    return settings_summary_height_dips + (estimate > room ? settings_summary_line_dips : 0);
}

// 節の見出し（機能の節は要約も）。前の節があれば下の空きを先に足す。
void push_heading(SettingsLayout &layout, std::int32_t &y, SettingsSection section,
                  std::int32_t estimate)
{
    const auto dpi = layout.dpi;
    if (!layout.items.empty())
    {
        y += to_pixels(section_bottom_dips, dpi);
    }
    push_item(layout, y,
              SettingsLayoutItem{SettingsItemKind::heading, nowhere, std::to_underlying(section),
                                 SettingsControl::fixed},
              heading_height_dips);
    if (section != SettingsSection::features)
    {
        return;
    }
    y += to_pixels(summary_top_dips, dpi);
    push_item(layout, y,
              SettingsLayoutItem{SettingsItemKind::summary, nowhere, 0, SettingsControl::fixed},
              summary_height(layout, estimate));
    y += to_pixels(summary_note_gap_dips, dpi);
    push_item(
        layout, y,
        SettingsLayoutItem{SettingsItemKind::summary_note, nowhere, 0, SettingsControl::fixed},
        summary_note_height_dips);
    y += to_pixels(summary_bottom_dips, dpi);
}

// 送る前の位置で全部を積み、内容の下端を返す。
[[nodiscard]] std::int32_t stacked(SettingsLayout &layout, const SettingsLayoutRequest &request)
{
    std::int32_t y = layout.list.top + to_pixels(list_top_dips, layout.dpi);
    for (std::size_t index = 0; index < request.rows.size(); ++index)
    {
        const auto &row = request.rows[index];
        if (index == 0 || request.rows[index - 1].section != row.section)
        {
            push_heading(layout, y, row.section, request.widths.estimate);
        }
        push_item(layout, y, SettingsLayoutItem{SettingsItemKind::row, nowhere, index, row.control},
                  row.control == SettingsControl::fold ? settings_fold_height_dips
                                                       : settings_row_height_dips);
    }
    if (layout.items.empty())
    {
        push_item(layout, y,
                  SettingsLayoutItem{SettingsItemKind::empty, nowhere, 0, SettingsControl::fixed},
                  empty_height_dips);
    }
    return y + to_pixels(list_bottom_dips, layout.dpi);
}

// 要約の中の組のボタン（下の段に左から）。
void place_presets(SettingsLayout &layout, const LayoutRect &summary,
                   const SettingsTextWidths &widths)
{
    const auto dpi = layout.dpi;
    const auto bottom = summary.bottom - to_pixels(summary_padding_dips, dpi);
    const auto top = bottom - to_pixels(preset_height_dips, dpi);
    std::int32_t x = summary.left + to_pixels(summary_padding_dips, dpi);
    for (std::size_t index = 0; index < layout.presets.size(); ++index)
    {
        const auto width = widths.presets.at(index) + (to_pixels(preset_padding_dips, dpi) * 2);
        layout.presets.at(index) = LayoutRect{x, top, x + width, bottom};
        x += width + to_pixels(chip_gap_dips, dpi);
    }
}

// 行の領域の中身を積んで、送りを切り詰めてからその分だけ上へずらす。
void place_items(SettingsLayout &layout, const SettingsLayoutRequest &request)
{
    const auto content_bottom = stacked(layout, request);
    layout.scroll_limit = std::max(0, content_bottom - layout.list.bottom);
    layout.scroll = std::clamp(request.scroll, 0, layout.scroll_limit);
    layout.presets.fill(nowhere);
    for (auto &item : layout.items)
    {
        item.box.top -= layout.scroll;
        item.box.bottom -= layout.scroll;
        if (item.kind == SettingsItemKind::summary)
        {
            place_presets(layout, item.box, request.widths);
        }
    }
}

// 行の中の当たり（段の上下の − ＋・選ぶ欄・ほかは行）。
[[nodiscard]] SettingsTarget row_target(const SettingsLayout &layout,
                                        const SettingsLayoutItem &item, std::int32_t x,
                                        std::int32_t y) noexcept
{
    const auto box = settings_control_box(layout, item);
    switch (item.control)
    {
    case SettingsControl::stepper:
        if (contains(settings_step_box(layout, box, false), x, y))
        {
            return SettingsTarget{SettingsPart::row_minus, item.index};
        }
        if (contains(settings_step_box(layout, box, true), x, y))
        {
            return SettingsTarget{SettingsPart::row_plus, item.index};
        }
        break;
    case SettingsControl::choice:
    case SettingsControl::action:
        if (contains(box, x, y))
        {
            return SettingsTarget{SettingsPart::row_value, item.index};
        }
        break;
    case SettingsControl::toggle:
    case SettingsControl::fixed:
    case SettingsControl::readonly:
    case SettingsControl::blank:
    case SettingsControl::fold:
        break;
    }
    return SettingsTarget{SettingsPart::row, item.index};
}

// 行の領域の中の当たり（組・行）。無ければ値なし。
[[nodiscard]] std::optional<SettingsTarget> list_target(const SettingsLayout &layout,
                                                        std::int32_t x, std::int32_t y) noexcept
{
    for (std::size_t index = 0; index < layout.presets.size(); ++index)
    {
        if (contains(layout.presets.at(index), x, y))
        {
            return SettingsTarget{SettingsPart::preset, index};
        }
    }
    for (const auto &item : layout.items)
    {
        if (item.kind == SettingsItemKind::row && contains(item.box, x, y))
        {
            return row_target(layout, item, x, y);
        }
    }
    return std::nullopt;
}

// 見えている行 row の矩形の位置（items の中）。無ければ items の大きさ。
[[nodiscard]] std::size_t item_of_row(const SettingsLayout &layout, std::size_t row) noexcept
{
    std::size_t at = 0;
    while (at < layout.items.size() &&
           (layout.items.at(at).kind != SettingsItemKind::row || layout.items.at(at).index != row))
    {
        ++at;
    }
    return at;
}

// 行の矩形の上端。節の最初の行は、直前に続く見出しと要約の上端まで広げる。
[[nodiscard]] std::int32_t revealed_top(const SettingsLayout &layout, std::size_t at) noexcept
{
    std::size_t first = at;
    while (first > 0 && layout.items.at(first - 1).kind != SettingsItemKind::row)
    {
        --first;
    }
    const auto top = layout.items.at(first).box.top;
    const bool fits = layout.items.at(at).box.bottom - top <= height_of(layout.list);
    return fits ? top : layout.items.at(at).box.top;
}

// 見えている行の数。
[[nodiscard]] std::size_t row_count(const SettingsLayout &layout) noexcept
{
    return static_cast<std::size_t>(
        std::ranges::count(layout.items, SettingsItemKind::row, &SettingsLayoutItem::kind));
}

// 見えている行 row の上端（row は行の数より小さい）。
[[nodiscard]] std::int32_t row_top(const SettingsLayout &layout, std::size_t row) noexcept
{
    return layout.items.at(item_of_row(layout, row)).box.top;
}

// PageUp / PageDown の 1 歩（端では動けない）。
[[nodiscard]] bool steppable(std::size_t at, std::size_t count, bool down) noexcept
{
    return down ? at + 1 < count : at > 0;
}

[[nodiscard]] std::size_t stepped(std::size_t at, bool down) noexcept
{
    return down ? at + 1 : at - 1;
}
// 行の右端の部品の幅と高さ（DIP）。
[[nodiscard]] std::pair<std::int32_t, std::int32_t> control_size(SettingsControl control) noexcept
{
    switch (control)
    {
    case SettingsControl::toggle:
        return {settings_switch_width_dips, settings_switch_height_dips};
    case SettingsControl::fixed:
        return {settings_fixed_width_dips, settings_switch_height_dips};
    case SettingsControl::choice:
    case SettingsControl::stepper:
    case SettingsControl::readonly:
        return {settings_value_width_dips, settings_value_height_dips};
    case SettingsControl::action:
        return {settings_action_width_dips, settings_action_height_dips};
    case SettingsControl::blank:
    case SettingsControl::fold:
        return {0, 0};
    }
    std::unreachable();
}
} // namespace

SettingsControl settings_control_of(const SettingsSubject &subject, bool switchable) noexcept
{
    if (std::holds_alternative<EditorFeature>(subject))
    {
        return switchable ? SettingsControl::toggle : SettingsControl::fixed;
    }
    return value_control(std::get<SettingsValue>(subject));
}

SettingsLayout settings_layout(const SettingsLayoutRequest &request)
{
    SettingsLayout layout = framed(request);
    const auto chips_bottom = place_chips(layout, request.widths);
    place_footer(layout, request.widths);
    const auto line = chips_bottom + to_pixels(block_gap_dips, layout.dpi);
    layout.divider =
        LayoutRect{layout.border.right, line, layout.panel.right, line + to_pixels(1, layout.dpi)};
    layout.list = LayoutRect{layout.border.right, layout.divider.bottom, layout.panel.right,
                             std::max(layout.divider.bottom, layout.footer_line.top)};
    place_items(layout, request);
    return layout;
}

SettingsTarget settings_hit(const SettingsLayout &layout, std::int32_t x, std::int32_t y) noexcept
{
    if (!contains(layout.panel, x, y))
    {
        return SettingsTarget{SettingsPart::outside, 0};
    }
    if (contains(layout.close, x, y))
    {
        return SettingsTarget{SettingsPart::close, 0};
    }
    if (contains(layout.search, x, y))
    {
        return SettingsTarget{SettingsPart::search, 0};
    }
    for (std::size_t index = 0; index < layout.chips.size(); ++index)
    {
        if (contains(layout.chips.at(index), x, y))
        {
            return SettingsTarget{SettingsPart::chip, index};
        }
    }
    if (contains(layout.revert, x, y))
    {
        return SettingsTarget{SettingsPart::revert, 0};
    }
    if (contains(layout.close_button, x, y))
    {
        return SettingsTarget{SettingsPart::close_button, 0};
    }
    const auto listed = contains(layout.list, x, y) ? list_target(layout, x, y) : std::nullopt;
    return listed.value_or(SettingsTarget{SettingsPart::panel, 0});
}

LayoutRect settings_control_box(const SettingsLayout &layout,
                                const SettingsLayoutItem &row) noexcept
{
    const auto dpi = layout.dpi;
    const auto [wide, tall] = control_size(row.control);
    const auto width = to_pixels(wide, dpi);
    const auto height = to_pixels(tall, dpi);
    const auto padding = to_pixels(settings_row_padding_dips, dpi);
    const auto right = row.box.right - padding;
    const auto left = std::max(row.box.left + padding, right - width);
    const auto top = row.box.top + ((height_of(row.box) - height) / 2);
    return LayoutRect{left, top, right, top + height};
}

LayoutRect settings_step_box(const SettingsLayout &layout, const LayoutRect &box,
                             bool plus) noexcept
{
    const auto dpi = layout.dpi;
    const auto width = to_pixels(step_width_dips, dpi);
    const auto height = to_pixels(step_height_dips, dpi);
    const auto inset = to_pixels(step_inset_dips, dpi);
    const auto top = box.top + ((height_of(box) - height) / 2);
    const auto left = plus ? box.right - inset - width : box.left + inset;
    return LayoutRect{left, top, left + width, top + height};
}

std::int32_t settings_scroll_revealing(const SettingsLayout &layout, std::size_t row) noexcept
{
    const auto at = item_of_row(layout, row);
    if (at == layout.items.size())
    {
        return layout.scroll;
    }
    // 最初と最後のものは、行の領域の上下の余白まで見せる（端まで送る）。
    const bool first = revealed_top(layout, at) == layout.items.front().box.top;
    const auto top = revealed_top(layout, at) - (first ? to_pixels(list_top_dips, layout.dpi) : 0);
    const auto bottom =
        layout.items.at(at).box.bottom +
        (at + 1 == layout.items.size() ? to_pixels(list_bottom_dips, layout.dpi) : 0);
    std::int32_t moved = layout.scroll;
    if (top < layout.list.top)
    {
        moved -= layout.list.top - top;
    }
    else if (bottom > layout.list.bottom)
    {
        moved += bottom - layout.list.bottom;
    }
    return std::clamp(moved, 0, layout.scroll_limit);
}

std::int32_t settings_scroll_revealing_section(const SettingsLayout &layout,
                                               SettingsSection section) noexcept
{
    const auto heading = std::ranges::find_if(layout.items,
                                              [section](const SettingsLayoutItem &item)
                                              {
                                                  return item.kind == SettingsItemKind::heading &&
                                                         item.index == std::to_underlying(section);
                                              });
    if (heading == layout.items.end())
    {
        return layout.scroll;
    }
    const auto next = std::ranges::find(heading + 1, layout.items.end(), SettingsItemKind::heading,
                                        &SettingsLayoutItem::kind);
    const auto bottom = (next - 1)->box.bottom +
                        (next == layout.items.end() ? to_pixels(list_bottom_dips, layout.dpi) : 0);
    std::int32_t moved = layout.scroll;
    if (heading->box.top < layout.list.top || bottom - heading->box.top > height_of(layout.list))
    {
        moved -= layout.list.top - heading->box.top;
    }
    else if (bottom > layout.list.bottom)
    {
        moved += bottom - layout.list.bottom;
    }
    return std::clamp(moved, 0, layout.scroll_limit);
}

std::int32_t settings_scroll_by(const SettingsLayout &layout, std::int32_t from,
                                std::int32_t lines) noexcept
{
    const auto moved = from + (lines * to_pixels(settings_wheel_line_dips, layout.dpi));
    return std::clamp(moved, 0, layout.scroll_limit);
}

std::optional<std::size_t> settings_paged_row(const SettingsLayout &layout,
                                              std::optional<std::size_t> focused,
                                              bool down) noexcept
{
    const auto count = row_count(layout);
    if (count == 0)
    {
        return std::nullopt;
    }
    if (!focused.has_value() || focused.value() >= count)
    {
        return 0;
    }
    const auto row = to_pixels(settings_row_height_dips, layout.dpi);
    const auto page = std::max(height_of(layout.list) - row, row);
    const auto origin = row_top(layout, focused.value());
    std::size_t at = focused.value();
    // 1 ページの中でいちばん遠い行まで進む。
    while (steppable(at, count, down) &&
           std::abs(row_top(layout, stepped(at, down)) - origin) <= page)
    {
        at = stepped(at, down);
    }
    // 見出しや要約を挟んで次の行が 1 ページより遠くても、1 行は動く。
    if (at == focused.value() && steppable(at, count, down))
    {
        at = stepped(at, down);
    }
    return at;
}
} // namespace nenenib::core
