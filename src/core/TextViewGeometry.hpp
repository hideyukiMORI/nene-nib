#pragma once

#include <cstdint>

namespace nenenib::core
{
// 本文の幅と行高は物理画素。幅0は窓から最初の寸法が届く前を表す。
// average_advance は本文の書式の 1 字の送り幅（物理画素・ui が格子の 1 升と同じ所で測る）。
// text の表示の巨大な行の窓の始まりの x を「仮想桁 × 字幅」で見積もる（ADR 0139 の決定 3・5）。
// 0 は測る前で、見積もりは 1 画素とみなす。
struct TextViewGeometry
{
    std::int32_t width = 0;
    std::int32_t line_height = 1;
    std::uint32_t dpi = 96;
    float average_advance = 0;
};

[[nodiscard]] constexpr bool operator==(TextViewGeometry left, TextViewGeometry right) noexcept
{
    return left.width == right.width && left.line_height == right.line_height &&
           left.dpi == right.dpi && left.average_advance == right.average_advance;
}
} // namespace nenenib::core
