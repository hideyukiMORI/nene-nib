#pragma once

#include "Offset.hpp"
#include "VimBlockEdit.hpp"
#include "VimMarkPosition.hpp"

#include <optional>
#include <vector>

namespace nenenib::core
{
// 矩形の文字置換（`r`・ADR 0035 の決定 4）。行ごとに「覆った桁の数」だけ同じ文字を書く
// （Tab の上では桁の数ぶん・Issue #112 で実測）。caret は矩形の左上。
struct VimReplaceBlock
{
    std::vector<VimBlockEdit> edits;
    Offset caret;
    // 編集の後の本文の `'.`（と変更の列の最後の位置）。値が無ければ controller が畳んだ Edit から
    // 決める（矩形は Vim も上の行の 1 回の changed_lines）。Ex の行の命令は Vim が命令ごとに
    // changed_lines を呼ぶ位置で、`:s` は最初に置き換えた行の桁 0、`:g` は最後に流した命令の位置
    // （ADR 0168 の段 3a の追補 9・実測 #1654）。
    std::optional<VimMarkPosition> last_change = std::nullopt;
};
} // namespace nenenib::core
