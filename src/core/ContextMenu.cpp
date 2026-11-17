#include "ContextMenu.hpp"

#include "ContextMenuNeed.hpp"
#include "ContextMenuSlot.hpp"
#include "DevicePixels.hpp"
#include "KeyChord.hpp"
#include "OperationBindings.hpp"
#include "OperationText.hpp"
#include "OperationTexts.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
namespace
{
// メニューに載らない行の置き場（並べる順の外）。
constexpr ContextMenuSlot outside_menu{0xFF, 0xFF, ContextMenuNeed::nothing};

[[nodiscard]] constexpr ContextMenuSlot slot_of(const OperationText &text) noexcept
{
    return text.context_menu.value_or(outside_menu);
}

// 群と順を 1 つの数にした並べる鍵。
[[nodiscard]] constexpr std::uint32_t rank_of(const OperationText &text) noexcept
{
    const auto slot = slot_of(text);
    return (std::uint32_t{slot.group} << 8U) | slot.order;
}

// メニューの中で text より前に並ぶ行の数（text の行の番号）。
[[nodiscard]] consteval std::size_t position_of(const OperationText &text) noexcept
{
    std::size_t before = 0;
    for (const auto &other : operation_texts)
    {
        before += other.context_menu.has_value() && rank_of(other) < rank_of(text) ? 1U : 0U;
    }
    return before;
}

// 表の列 context_menu を群と順で並べた操作の列。
[[nodiscard]] consteval std::array<EditorOperation, context_menu_size> ordered_operations() noexcept
{
    std::array<EditorOperation, context_menu_size> ordered{};
    for (const auto &text : operation_texts)
    {
        if (text.context_menu.has_value())
        {
            ordered.at(position_of(text)) = text.operation;
        }
    }
    return ordered;
}

constexpr auto menu_operations = ordered_operations();

// 群 group の行がある。
[[nodiscard]] consteval bool group_used(std::size_t group) noexcept
{
    for (const auto &text : operation_texts)
    {
        if (text.context_menu.has_value() && slot_of(text).group == group)
        {
            return true;
        }
    }
    return false;
}

// 同じ群と順の行を 2 つ持たない（並べた番号が行ごとに違う）。
[[nodiscard]] consteval bool slots_distinct() noexcept
{
    std::array<bool, context_menu_size> taken{};
    for (const auto &text : operation_texts)
    {
        if (!text.context_menu.has_value())
        {
            continue;
        }
        if (taken.at(position_of(text)))
        {
            return false;
        }
        taken.at(position_of(text)) = true;
    }
    return true;
}

// 群は 0 から詰めて振る（どの群にも行がある）。
[[nodiscard]] consteval bool groups_packed() noexcept
{
    for (std::size_t group = 0; group <= context_menu_separator_count; ++group)
    {
        if (!group_used(group))
        {
            return false;
        }
    }
    return true;
}

static_assert(slots_distinct() && groups_packed(),
              "context menu slots are distinct and the groups are numbered from 0 without gaps");

[[nodiscard]] bool need_met(ContextMenuNeed need, const ContextMenuSituation &situation) noexcept
{
    switch (need)
    {
    case ContextMenuNeed::nothing:
        return true;
    case ContextMenuNeed::selection:
        return situation.selected;
    case ContextMenuNeed::editable:
        return situation.editable;
    case ContextMenuNeed::editable_selection:
        return situation.editable && situation.selected;
    }
    std::unreachable();
}

[[nodiscard]] std::int32_t separator_height(std::uint32_t dpi) noexcept
{
    return to_pixels(1, dpi) + (to_pixels(context_menu_gap_dips, dpi) * 2);
}

// 開く点から右（下）へ size を置き、bounds の端を越えれば点から左（上）へ。どちらにも入らなければ
// 遠い端に寄せ、bounds より大きければ始まりに寄せる。
[[nodiscard]] std::int32_t placed_start(std::int32_t at, std::int32_t size, std::int32_t low,
                                        std::int32_t high) noexcept
{
    if (at + size <= high)
    {
        return std::max(low, at);
    }
    return std::max(low, at - size >= low ? at - size : high - size);
}
} // namespace

std::array<ContextMenuItem, context_menu_size>
context_menu_items(const ContextMenuSituation &situation) noexcept
{
    std::array<ContextMenuItem, context_menu_size> items{};
    for (std::size_t index = 0; index < menu_operations.size(); ++index)
    {
        const auto operation = menu_operations.at(index);
        const auto slot = slot_of(operation_text(operation));
        const bool separated =
            index > 0 && slot_of(operation_text(menu_operations.at(index - 1))).group != slot.group;
        items.at(index) = ContextMenuItem{operation,
                                          operation_available(operation, situation.mode) &&
                                              need_met(slot.need, situation),
                                          separated};
    }
    return items;
}

std::string context_menu_key(EditorOperation operation, EditMode mode)
{
    switch (mode)
    {
    case EditMode::ordinary:
    {
        const auto chord = shown_chord(operation, mode);
        return chord.has_value() ? key_chord_label(chord.value()) : std::string{};
    }
    case EditMode::vim:
        return std::string{operation_text(operation).vim_hint};
    }
    std::unreachable();
}

ContextMenuLayout context_menu_layout(const ContextMenuRequest &request) noexcept
{
    const auto dpi = request.dpi;
    const auto padding = to_pixels(context_menu_padding_dips, dpi);
    const auto key_gap = request.key_width > 0 ? to_pixels(context_menu_key_gap_dips, dpi) : 0;
    const auto width =
        std::max(to_pixels(context_menu_min_width_dips, dpi),
                 padding + request.name_width + key_gap + request.key_width + padding);
    const auto row_height = to_pixels(context_menu_row_dips, dpi);
    const auto gap = to_pixels(context_menu_gap_dips, dpi);
    const auto height =
        (gap * 2) + (row_height * static_cast<std::int32_t>(context_menu_size)) +
        (separator_height(dpi) * static_cast<std::int32_t>(context_menu_separator_count));
    const auto left = placed_start(request.x, width, request.bounds.left, request.bounds.right);
    const auto top = placed_start(request.y, height, request.bounds.top, request.bounds.bottom);
    ContextMenuLayout layout{LayoutRect{left, top, left + width, top + height}, {}, {}, dpi};
    const auto items = context_menu_items(ContextMenuSituation{EditMode::ordinary, false, false});
    auto y = top + gap;
    std::size_t separator = 0;
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        if (items.at(index).separated)
        {
            const auto line = y + gap;
            layout.separators.at(separator) =
                LayoutRect{left + padding, line, left + width - padding, line + to_pixels(1, dpi)};
            ++separator;
            y += separator_height(dpi);
        }
        layout.rows.at(index) = LayoutRect{left, y, left + width, y + row_height};
        y += row_height;
    }
    return layout;
}

std::optional<std::size_t> context_menu_hit(const ContextMenuLayout &layout, std::int32_t x,
                                            std::int32_t y) noexcept
{
    for (std::size_t index = 0; index < layout.rows.size(); ++index)
    {
        if (contains(layout.rows.at(index), x, y))
        {
            return index;
        }
    }
    return std::nullopt;
}

LayoutRect context_menu_row_inside(const ContextMenuLayout &layout, std::size_t index) noexcept
{
    const auto &row = layout.rows.at(std::min(index, layout.rows.size() - 1));
    const auto padding = to_pixels(context_menu_padding_dips, layout.dpi);
    const auto left = std::min(row.left + padding, row.right);
    return LayoutRect{left, row.top, std::max(left, row.right - padding), row.bottom};
}
} // namespace nenenib::core
