#pragma once

#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::core
{
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
[[nodiscard]] std::size_t palette_first_visible(const PaletteLayout &layout,
                                                std::size_t selected) noexcept;
} // namespace nenenib::core
