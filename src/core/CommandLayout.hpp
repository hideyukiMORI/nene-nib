#pragma once

#include "LayoutRect.hpp"
#include "StatusBarLayout.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// `:` の補完の面に並ぶ行の数の上限（ADR 0022・ADR 0150 の決定 1）。配置と CommandLine の窓が読む。
inline constexpr std::size_t command_completion_row_limit = 6;
// 入力行と補完の候補の文字を矩形の左右から内側へ寄せる幅（DIP・#786）。両方がこの 1 つを読む。
// Ctrl+P の面の行（PaletteLayout の 8 DIP）とは別の正本で、揃えない。
inline constexpr std::int32_t command_text_inset_dip = 12;

struct CommandLayout
{
    LayoutRect input;
    LayoutRect panel;
    std::int32_t row_height;
    std::size_t visible_rows;
};

[[nodiscard]] CommandLayout command_layout(const StatusBarLayout &status, std::uint32_t dpi,
                                           std::size_t candidates) noexcept;

// 行の矩形から左右 command_text_inset_dip 内側の文字の矩形。幅が足りなければ幅 0（#786）。
[[nodiscard]] LayoutRect command_row_label(const LayoutRect &row, std::uint32_t dpi) noexcept;

// 入力行の文字の矩形。候補の行と同じ内側の幅で寄せる（#786）。
[[nodiscard]] LayoutRect command_input_label(const CommandLayout &layout,
                                             std::uint32_t dpi) noexcept;
} // namespace nenenib::core
