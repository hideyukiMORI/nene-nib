#pragma once

#include "PreviewImages.hpp"

#include <cstddef>

namespace nenenib::ui::win32
{
// PreviewBlockTops が測る範囲（ADR 0155 の決定 7・#811 のレビュー R1）。first は枠の上端に置く
// 要素（錨の要素）、width と height は枠の幅と高さ（物理画素）。first から順に測り、積んだ高さが
// height を超えたら止める。images は画像の状態（ADR 0155 の追補 62・null なら画像の節が
// 無い）で、読めた画像は箱の大きさで測る。nudge は first を置き始める位置から枠の上端までの
// 物理画素（ADR 0155 の決定 7 の 2026-11-20 の追補・#1894・first の上端は枠の上端の nudge 上）。
// through より前の要素は枠の高さを超えても置く（送りを畳むときに次の要素の上端を測る・既定の 0 は
// 高さだけで止める）。どれも互いに独立して妥当なので公開 aggregate（CPP-003）。
struct PreviewViewport
{
    std::size_t first;
    float width;
    float height;
    const application::PreviewImages *images = nullptr;
    float nudge = 0.0F;
    std::size_t through = 0;
};
} // namespace nenenib::ui::win32
