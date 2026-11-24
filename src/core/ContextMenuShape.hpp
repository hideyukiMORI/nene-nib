#pragma once

#include "ContextMenuKind.hpp"
#include "ContextMenuSlot.hpp"
#include "OperationText.hpp"
#include "OperationTexts.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace nenenib::core
{
// メニューの寸法（ADR 0078 の追補 #1051・設計席の決め）。DIP。本文とタブのメニューで同じ。
inline constexpr std::int32_t context_menu_row_dips = 30;
inline constexpr std::int32_t context_menu_padding_dips = 12;
inline constexpr std::int32_t context_menu_min_width_dips = 200;
// 区切り線の上下の空き。面の上下の内側の空きも同じ値。
inline constexpr std::int32_t context_menu_gap_dips = 4;
// 名前と鍵の表示の間の最小の空き。
inline constexpr std::int32_t context_menu_key_gap_dips = 24;
// 面の角丸（Ctrl+P の面と同じ 10）・選んだ行の角丸（6）と、選んだ行の地の左右の内側（4）。設定の
// 選ぶ欄の一覧も同じ値を引く（#1926）。
inline constexpr float context_menu_radius_dips = 10.0F;
inline constexpr float context_menu_row_radius_dips = 6.0F;
inline constexpr std::int32_t context_menu_row_inset_dips = 4;

// 種類 kind のメニューでの置き場（表の列 context_menu か tab_menu・#1790）。
[[nodiscard]] constexpr const std::optional<ContextMenuSlot> &
menu_slot_of(const OperationText &text, ContextMenuKind kind) noexcept
{
    switch (kind)
    {
    case ContextMenuKind::body:
        return text.context_menu;
    case ContextMenuKind::tab:
        return text.tab_menu;
    }
    std::unreachable();
}

namespace detail
{
[[nodiscard]] consteval std::size_t count_menu_rows(ContextMenuKind kind) noexcept
{
    std::size_t count = 0;
    for (const auto &text : operation_texts)
    {
        count += menu_slot_of(text, kind).has_value() ? 1U : 0U;
    }
    return count;
}

// 群の数 - 1（群は 0 から詰めて振る）。
[[nodiscard]] consteval std::size_t count_menu_separators(ContextMenuKind kind) noexcept
{
    std::size_t last = 0;
    for (const auto &text : operation_texts)
    {
        const auto &slot = menu_slot_of(text, kind);
        if (slot.has_value() && slot.value().group > last)
        {
            last = slot.value().group;
        }
    }
    return last;
}
} // namespace detail

// 本文のメニューの項目の数と区切り線の数。操作の表の列 context_menu から数える。
inline constexpr std::size_t context_menu_size = detail::count_menu_rows(ContextMenuKind::body);
inline constexpr std::size_t context_menu_separator_count =
    detail::count_menu_separators(ContextMenuKind::body);
// タブのメニューの項目の数と区切り線の数（列 tab_menu・#1790）。
inline constexpr std::size_t tab_menu_size = detail::count_menu_rows(ContextMenuKind::tab);
inline constexpr std::size_t tab_menu_separator_count =
    detail::count_menu_separators(ContextMenuKind::tab);
// 配置が持てる行と区切り線の数（どちらの種類も入る大きさ）。
inline constexpr std::size_t context_menu_row_capacity = std::max(context_menu_size, tab_menu_size);
inline constexpr std::size_t context_menu_separator_capacity =
    std::max(context_menu_separator_count, tab_menu_separator_count);
} // namespace nenenib::core
