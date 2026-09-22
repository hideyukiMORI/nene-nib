#pragma once

#include "Offset.hpp"
#include "VimBlockEdit.hpp"

#include <vector>

namespace nenenib::core
{
// 矩形の削除（`d` `x`・ADR 0035 の決定 3・4）。行ごとの置き換えを上の行から並べる。controller は
// 後ろの行から順に写し、前後で履歴を閉じて undo 1 単位にする。caret は矩形の左上。
struct VimRemoveBlock
{
    std::vector<VimBlockEdit> edits;
    Offset caret;
};
} // namespace nenenib::core
