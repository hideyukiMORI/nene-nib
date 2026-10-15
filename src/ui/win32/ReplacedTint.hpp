#pragma once
#include <cstddef>
#include <d2d1.h>
namespace nenenib::ui::win32
{
// 置き換えた文字（`^M`・`<200d>`）を muted で描き直す 1 つの切り抜き（#531・ADR 0040 の決定 4）。
// clip は layout の座標（描く側が行の原点を足す）。描くのは保持した字形 run の [first, last) だけ。
struct ReplacedTint
{
    D2D1_RECT_F clip;
    std::size_t first;
    std::size_t last;
};
} // namespace nenenib::ui::win32
