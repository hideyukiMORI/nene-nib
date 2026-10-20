#pragma once

#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::core
{
// 面に並ぶ行の数の上限（ADR 0062 の決定 2）。配置の見えている行数と面の窓の行数（ADR 0150 の
// 決定 1）が読む。
inline constexpr std::size_t palette_row_limit = 8;

struct PaletteLayout
{
    LayoutRect panel;
    LayoutRect input;
    LayoutRect rows;
    LayoutRect footer;
    std::int32_t row_height;
    std::size_t visible_rows;
};

[[nodiscard]] PaletteLayout palette_layout(std::int32_t width, std::int32_t height,
                                           std::uint32_t dpi, std::size_t choices) noexcept;
[[nodiscard]] LayoutRect palette_row(const PaletteLayout &layout,
                                     std::size_t visible_index) noexcept;
[[nodiscard]] std::optional<std::size_t> palette_hit(const PaletteLayout &layout, std::int32_t x,
                                                     std::int32_t y) noexcept;
// 窓の大きさで見えている行数（上限 palette_row_limit・ADR 0150 の決定 1）。候補の件数に依らない
// 値で、application が面の窓の行数として渡す。描く行数は palette_layout の visible_rows。
[[nodiscard]] std::size_t palette_window_rows(std::int32_t width, std::int32_t height,
                                              std::uint32_t dpi) noexcept;

// 行の右端の補足の欄（ADR 0060 の決定 9）。行の内側（左右 16 DIP）の右端から幅 112 DIP。
// 内側が 補足 112 + すき間 12 + 名前の最小 120 DIP より狭ければ幅 0（補足を出さない）。
[[nodiscard]] LayoutRect palette_row_note(const LayoutRect &row, std::uint32_t dpi) noexcept;
// 名前と場所の欄。`noted` で補足の欄が幅を持てばその左 12 DIP まで、そうでなければ行の内側の全部。
[[nodiscard]] LayoutRect palette_row_label(const LayoutRect &row, std::uint32_t dpi,
                                           bool noted) noexcept;
// 検索欄の右半分（記号の案内の欄）。160 DIP 相当より狭ければ幅 0。
[[nodiscard]] LayoutRect palette_input_hint(const PaletteLayout &layout) noexcept;
} // namespace nenenib::core
