#pragma once

#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 本文の帯の寸法（docs/design/2026-09-15-look.md 第 2 節）を物理画素で表した値。
// 見えている行数もここが決める。窓（何行送るか）と描画（どこに描くか）が同じ数を使う（ARC-001）。
struct BodyLayout
{
    LayoutRect band;
    LayoutRect gutter;
    LayoutRect content;
    std::int32_t line_height;
    std::int32_t caret_width;
    std::size_t visible_lines;
};

[[nodiscard]] BodyLayout body_layout(std::int32_t width, std::int32_t height,
                                     std::uint32_t dpi) noexcept;

[[nodiscard]] LayoutRect body_line_rect(const BodyLayout &layout, std::size_t index) noexcept;
} // namespace nenenib::core
