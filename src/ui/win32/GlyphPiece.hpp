#pragma once
#include <cstddef>
#include <d2d1.h>

namespace nenenib::ui::win32
{
// 保持した字形 run の部分窓 [first, last) と、その窓を描く色（ADR 0156 の決定 11）。
struct GlyphPiece
{
    std::size_t first;
    std::size_t last;
    D2D1_COLOR_F color;
};
} // namespace nenenib::ui::win32
