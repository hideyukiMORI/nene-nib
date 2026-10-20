#pragma once

#include "LayoutRect.hpp"
#include "StatusBarLayout.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// `:` の補完の面に並ぶ行の数の上限（ADR 0022・ADR 0150 の決定 1）。配置と CommandLine の窓が読む。
inline constexpr std::size_t command_completion_row_limit = 6;

struct CommandLayout
{
    LayoutRect input;
    LayoutRect panel;
    std::int32_t row_height;
    std::size_t visible_rows;
};

[[nodiscard]] CommandLayout command_layout(const StatusBarLayout &status, std::uint32_t dpi,
                                           std::size_t candidates) noexcept;
} // namespace nenenib::core
