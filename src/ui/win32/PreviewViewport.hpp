#pragma once

#include <cstddef>

namespace nenenib::ui::win32
{
// PreviewBlockTops が測る範囲（ADR 0155 の決定 7・#811 のレビュー R1）。first は枠の上端に置く
// 要素（錨の要素）、width と height は枠の幅と高さ（物理画素）。first から順に測り、積んだ高さが
// height を超えたら止める。3 つは互いに独立して妥当なので公開 aggregate（CPP-003）。
struct PreviewViewport
{
    std::size_t first;
    float width;
    float height;
};
} // namespace nenenib::ui::win32
