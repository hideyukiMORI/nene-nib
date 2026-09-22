#pragma once

#include "Offset.hpp"
#include "VimBlockEdit.hpp"

#include <vector>

namespace nenenib::core
{
// 矩形の文字置換（`r`・ADR 0035 の決定 4）。行ごとに「覆った桁の数」だけ同じ文字を書く
// （Tab の上では桁の数ぶん・Issue #112 で実測）。caret は矩形の左上。
struct VimReplaceBlock
{
    std::vector<VimBlockEdit> edits;
    Offset caret;
};
} // namespace nenenib::core
