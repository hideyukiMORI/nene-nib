#include "ContextMenu.hpp"

#include "ContextMenuKind.hpp"
#include "ContextMenuMode.hpp"
#include "ContextMenuNeed.hpp"
#include "ContextMenuSituation.hpp"
#include "ContextMenuSlot.hpp"
#include "DevicePixels.hpp"
#include "EditMode.hpp"
#include "KeyChord.hpp"
#include "OperationBindings.hpp"
#include "OperationText.hpp"
#include "OperationTexts.hpp"
#include "TabMenuSituation.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// メニューに載らない行の置き場（並べる順の外）。
constexpr ContextMenuSlot outside_menu{0xFF, 0xFF, ContextMenuNeed::nothing};

[[nodiscard]] constexpr ContextMenuSlot slot_of(const OperationText &text,
                                                ContextMenuKind kind) noexcept
{
    return menu_slot_of(text, kind).value_or(outside_menu);
}

// 群と順を 1 つの数にした並べる鍵。
[[nodiscard]] constexpr std::uint32_t rank_of(const OperationText &text,
                                              ContextMenuKind kind) noexcept
{
    const auto slot = slot_of(text, kind);
    return (std::uint32_t{slot.group} << 8U) | slot.order;
}

// kind のメニューの中で text より前に並ぶ行の数（text の行の番号）。
[[nodiscard]] consteval std::size_t position_of(const OperationText &text,
                                                ContextMenuKind kind) noexcept
{
    std::size_t before = 0;
    for (const auto &other : operation_texts)
    {
        const bool earlier =
            menu_slot_of(other, kind).has_value() && rank_of(other, kind) < rank_of(text, kind);
        before += earlier ? 1U : 0U;
    }
    return before;
}

// 表の列（Kind の列）を群と順で並べた操作の列。
template <ContextMenuKind Kind, std::size_t Size>
[[nodiscard]] consteval std::array<EditorOperation, Size> ordered_operations() noexcept
{
    std::array<EditorOperation, Size> ordered{};
    for (const auto &text : operation_texts)
    {
        if (menu_slot_of(text, Kind).has_value())
        {
            ordered.at(position_of(text, Kind)) = text.operation;
        }
    }
    return ordered;
}

constexpr auto menu_operations = ordered_operations<ContextMenuKind::body, context_menu_size>();
constexpr auto tab_operations = ordered_operations<ContextMenuKind::tab, tab_menu_size>();

// kind の群 group の行がある。
[[nodiscard]] consteval bool group_used(ContextMenuKind kind, std::size_t group) noexcept
{
    for (const auto &text : operation_texts)
    {
        if (menu_slot_of(text, kind).has_value() && slot_of(text, kind).group == group)
        {
            return true;
        }
    }
    return false;
}

// 同じ群と順の行を 2 つ持たない（並べた番号が行ごとに違う）。
template <ContextMenuKind Kind, std::size_t Size>
[[nodiscard]] consteval bool slots_distinct() noexcept
{
    std::array<bool, Size> taken{};
    for (const auto &text : operation_texts)
    {
        if (!menu_slot_of(text, Kind).has_value())
        {
            continue;
        }
        if (taken.at(position_of(text, Kind)))
        {
            return false;
        }
        taken.at(position_of(text, Kind)) = true;
    }
    return true;
}

// 群は 0 から詰めて振る（どの群にも行がある）。
[[nodiscard]] consteval bool groups_packed(ContextMenuKind kind, std::size_t separators) noexcept
{
    for (std::size_t group = 0; group <= separators; ++group)
    {
        if (!group_used(kind, group))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr bool tab_need(ContextMenuNeed need) noexcept
{
    switch (need)
    {
    case ContextMenuNeed::named:
    case ContextMenuNeed::loaded:
    case ContextMenuNeed::other_tabs:
    case ContextMenuNeed::right_tabs:
        return true;
    case ContextMenuNeed::nothing:
    case ContextMenuNeed::selection:
    case ContextMenuNeed::editable:
    case ContextMenuNeed::editable_selection:
        return false;
    }
    std::unreachable();
}

// 本文の行はタブの need を、タブの行は本文の need を持たない（nothing はどちらにも書ける）。
[[nodiscard]] consteval bool needs_belong() noexcept
{
    for (const auto &text : operation_texts)
    {
        if (text.context_menu.has_value() && tab_need(text.context_menu.value().need))
        {
            return false;
        }
        const bool body_need = text.tab_menu.has_value() &&
                               text.tab_menu.value().need != ContextMenuNeed::nothing &&
                               !tab_need(text.tab_menu.value().need);
        if (body_need)
        {
            return false;
        }
    }
    return true;
}

static_assert(slots_distinct<ContextMenuKind::body, context_menu_size>() &&
                  groups_packed(ContextMenuKind::body, context_menu_separator_count),
              "context menu slots are distinct and the groups are numbered from 0 without gaps");
static_assert(slots_distinct<ContextMenuKind::tab, tab_menu_size>() &&
                  groups_packed(ContextMenuKind::tab, tab_menu_separator_count),
              "tab menu slots are distinct and the groups are numbered from 0 without gaps");
static_assert(needs_belong(), "body rows carry body needs and tab rows carry tab needs");

[[nodiscard]] bool need_met(ContextMenuNeed need, bool selected, bool editable) noexcept
{
    switch (need)
    {
    case ContextMenuNeed::nothing:
        return true;
    case ContextMenuNeed::selection:
        return selected;
    case ContextMenuNeed::editable:
        return editable;
    case ContextMenuNeed::editable_selection:
        return editable && selected;
    // タブの need は本文の行に書けない（needs_belong）。
    case ContextMenuNeed::named:
    case ContextMenuNeed::loaded:
    case ContextMenuNeed::other_tabs:
    case ContextMenuNeed::right_tabs:
        return false;
    }
    std::unreachable();
}

// タブの項目が使えるか（#1790）。本文の need は nothing のほかタブの行に書けない（needs_belong）。
[[nodiscard]] bool tab_need_met(ContextMenuNeed need, const TabMenuSituation &situation) noexcept
{
    switch (need)
    {
    case ContextMenuNeed::nothing:
        return true;
    case ContextMenuNeed::named:
        return situation.named;
    case ContextMenuNeed::loaded:
        return situation.loaded;
    case ContextMenuNeed::other_tabs:
        return situation.others;
    case ContextMenuNeed::right_tabs:
        return situation.right;
    case ContextMenuNeed::selection:
    case ContextMenuNeed::editable:
    case ContextMenuNeed::editable_selection:
        return false;
    }
    std::unreachable();
}

// 項目が使えるか。通常モードと Vim の INSERT は表の need、全面プレビューは選択の要る項目（コピー）
// だけ、Vim の NORMAL と VISUAL はそのモードの鍵の列があること。
[[nodiscard]] bool usable(const OperationText &text, const ContextMenuSituation &situation) noexcept
{
    const auto need = slot_of(text, ContextMenuKind::body).need;
    switch (situation.mode)
    {
    case ContextMenuMode::ordinary:
    case ContextMenuMode::vim_insert:
        return need_met(need, situation.selected, true);
    case ContextMenuMode::reading:
        return need == ContextMenuNeed::selection && situation.selected;
    case ContextMenuMode::vim_normal:
        return !text.vim_normal_keys.empty();
    case ContextMenuMode::vim_visual:
        return !text.vim_visual_keys.empty();
    }
    std::unreachable();
}

[[nodiscard]] std::string ordinary_label(EditorOperation operation)
{
    const auto chord = shown_chord(operation, EditMode::ordinary);
    return chord.has_value() ? key_chord_label(chord.value()) : std::string{};
}

[[nodiscard]] std::int32_t separator_height(std::uint32_t dpi) noexcept
{
    return to_pixels(1, dpi) + (to_pixels(context_menu_gap_dips, dpi) * 2);
}

// 並べた操作の列の区切り線の印（行の上に線があるか・群の変わり目）。
template <std::size_t Size>
[[nodiscard]] constexpr std::array<bool, Size>
separated_rows(const std::array<EditorOperation, Size> &ordered, ContextMenuKind kind) noexcept
{
    std::array<bool, Size> separated{};
    for (std::size_t index = 1; index < Size; ++index)
    {
        separated.at(index) = slot_of(operation_text(ordered.at(index - 1)), kind).group !=
                              slot_of(operation_text(ordered.at(index)), kind).group;
    }
    return separated;
}

// 行 index の上に区切り線があるか。
[[nodiscard]] bool row_separated(ContextMenuKind kind, std::size_t index) noexcept
{
    switch (kind)
    {
    case ContextMenuKind::body:
        return separated_rows(menu_operations, kind).at(index);
    case ContextMenuKind::tab:
        return separated_rows(tab_operations, kind).at(index);
    }
    std::unreachable();
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
    const auto separated = separated_rows(menu_operations, ContextMenuKind::body);
    for (std::size_t index = 0; index < menu_operations.size(); ++index)
    {
        const auto operation = menu_operations.at(index);
        items.at(index) = ContextMenuItem{operation, usable(operation_text(operation), situation),
                                          separated.at(index)};
    }
    return items;
}

std::array<ContextMenuItem, tab_menu_size>
tab_menu_items(const TabMenuSituation &situation) noexcept
{
    std::array<ContextMenuItem, tab_menu_size> items{};
    const auto separated = separated_rows(tab_operations, ContextMenuKind::tab);
    for (std::size_t index = 0; index < tab_operations.size(); ++index)
    {
        const auto operation = tab_operations.at(index);
        items.at(index) = ContextMenuItem{
            operation,
            tab_need_met(slot_of(operation_text(operation), ContextMenuKind::tab).need, situation),
            separated.at(index)};
    }
    return items;
}

std::string tab_menu_key(EditorOperation operation, EditMode mode)
{
    const auto chord = shown_chord(operation, mode);
    return chord.has_value() ? key_chord_label(chord.value()) : std::string{};
}

std::string context_menu_key(EditorOperation operation, ContextMenuMode mode)
{
    switch (mode)
    {
    case ContextMenuMode::ordinary:
    case ContextMenuMode::reading:
        return ordinary_label(operation);
    case ContextMenuMode::vim_insert:
        // INSERT で効く鍵は通常モードと同じクリップボードと全選択の鍵だけ（検索・置換の鍵は Vim
        // の鍵）。
        return slot_of(operation_text(operation), ContextMenuKind::body).need ==
                       ContextMenuNeed::nothing
                   ? std::string{}
                   : ordinary_label(operation);
    case ContextMenuMode::vim_normal:
    case ContextMenuMode::vim_visual:
        return std::string{context_menu_vim_keys(operation, mode)};
    }
    std::unreachable();
}

std::string_view context_menu_vim_keys(EditorOperation operation, ContextMenuMode mode) noexcept
{
    const auto &text = operation_text(operation);
    switch (mode)
    {
    case ContextMenuMode::vim_normal:
        return text.vim_normal_keys;
    case ContextMenuMode::vim_visual:
        return text.vim_visual_keys;
    case ContextMenuMode::ordinary:
    case ContextMenuMode::reading:
    case ContextMenuMode::vim_insert:
        return {};
    }
    std::unreachable();
}

bool listed_vim_routed(EditorOperation operation, ContextMenuMode mode) noexcept
{
    const auto &text = operation_text(operation);
    const bool vim_resting =
        mode == ContextMenuMode::vim_normal || mode == ContextMenuMode::vim_visual;
    return vim_resting && text.context_menu.has_value() &&
           slot_of(text, ContextMenuKind::body).need != ContextMenuNeed::nothing;
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
    const bool tab = request.kind == ContextMenuKind::tab;
    const std::size_t rows = tab ? tab_menu_size : context_menu_size;
    const std::size_t separators = tab ? tab_menu_separator_count : context_menu_separator_count;
    const auto height = (gap * 2) + (row_height * static_cast<std::int32_t>(rows)) +
                        (separator_height(dpi) * static_cast<std::int32_t>(separators));
    const auto left = placed_start(request.x, width, request.bounds.left, request.bounds.right);
    const auto top = placed_start(request.y, height, request.bounds.top, request.bounds.bottom);
    ContextMenuLayout layout{
        LayoutRect{left, top, left + width, top + height}, {}, {}, dpi, rows, separators};
    auto y = top + gap;
    std::size_t separator = 0;
    for (std::size_t index = 0; index < rows; ++index)
    {
        if (row_separated(request.kind, index))
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
    for (std::size_t index = 0; index < layout.row_count; ++index)
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
    const auto &row = layout.rows.at(std::min(index, layout.row_count - 1));
    const auto padding = to_pixels(context_menu_padding_dips, layout.dpi);
    const auto left = std::min(row.left + padding, row.right);
    return LayoutRect{left, row.top, std::max(left, row.right - padding), row.bottom};
}
} // namespace nenenib::core
