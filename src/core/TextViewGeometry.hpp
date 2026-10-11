#pragma once

#include <cstdint>

namespace nenenib::core
{
// 本文の幅と行高は物理画素。幅0は窓から最初の寸法が届く前を表す。
struct TextViewGeometry
{
    std::int32_t width = 0;
    std::int32_t line_height = 1;
    std::uint32_t dpi = 96;
};

[[nodiscard]] constexpr bool operator==(TextViewGeometry left, TextViewGeometry right) noexcept
{
    return left.width == right.width && left.line_height == right.line_height &&
           left.dpi == right.dpi;
}
} // namespace nenenib::core
