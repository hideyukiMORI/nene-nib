#pragma once

#include "OperationTexts.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// メニューの寸法（ADR 0078 の追補 #1051・設計席の決め）。DIP。
inline constexpr std::int32_t context_menu_row_dips = 30;
inline constexpr std::int32_t context_menu_padding_dips = 12;
inline constexpr std::int32_t context_menu_min_width_dips = 200;
// 区切り線の上下の空き。面の上下の内側の空きも同じ値。
inline constexpr std::int32_t context_menu_gap_dips = 4;
// 名前と鍵の表示の間の最小の空き。
inline constexpr std::int32_t context_menu_key_gap_dips = 24;

namespace detail
{
[[nodiscard]] consteval std::size_t count_context_menu_rows() noexcept
{
    std::size_t count = 0;
    for (const auto &text : operation_texts)
    {
        count += text.context_menu.has_value() ? 1U : 0U;
    }
    return count;
}

// 群の数 - 1（群は 0 から詰めて振る）。
[[nodiscard]] consteval std::size_t count_context_menu_separators() noexcept
{
    std::size_t last = 0;
    for (const auto &text : operation_texts)
    {
        if (text.context_menu.has_value() && text.context_menu.value().group > last)
        {
            last = text.context_menu.value().group;
        }
    }
    return last;
}
} // namespace detail

// メニューの項目の数と区切り線の数。操作の表の列 context_menu から数える。
inline constexpr std::size_t context_menu_size = detail::count_context_menu_rows();
inline constexpr std::size_t context_menu_separator_count = detail::count_context_menu_separators();
} // namespace nenenib::core
