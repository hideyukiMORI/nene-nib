#pragma once

#include "FinderPreview.hpp"

#include <cstddef>

namespace nenenib::core
{
// 面の配置が窓の大きさのほかに読む値（ADR 0062・ADR 0189 の決定 1）。choices は候補の件数（off の
// 形の行の数を決める）、placement は置き方（automatic を含む・欄を持たない出どころは呼び出し元が
// palette_placement で off にする）。値だけを運ぶ公開 aggregate（CPP-003）。
struct PaletteContent
{
    std::size_t choices;
    FinderPreview placement;
};
} // namespace nenenib::core
