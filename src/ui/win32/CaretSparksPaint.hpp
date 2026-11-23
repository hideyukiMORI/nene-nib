#pragma once

#include <d2d1.h>

namespace nenenib::ui::win32
{
// 打ったときの粒の描き方（ADR 0192 の追補・#1979）。head は 1 つ目の色（キャレットの accent）、tint
// は 2 つ目の色（尾の先と同じ ime）、scale は 1 DIP の物理画素（粒のずれと大きさは DIP）。公開
// aggregate（ADR 0007）。
struct CaretSparksPaint
{
    D2D1_COLOR_F head;
    D2D1_COLOR_F tint;
    float scale;
};
} // namespace nenenib::ui::win32
