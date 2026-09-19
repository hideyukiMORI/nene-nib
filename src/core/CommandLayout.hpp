#pragma once

#include "LayoutRect.hpp"
#include "StatusBarLayout.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
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
