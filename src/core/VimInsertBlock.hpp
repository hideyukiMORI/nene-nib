#pragma once

#include "EditBoundary.hpp"
#include "Offset.hpp"
#include "VimBlockEdit.hpp"

#include <vector>

namespace nenenib::core
{
// 矩形レジスタの貼付（NORMAL の `p` `P`・ADR 0035 の決定 6）。行ごとの挿入を上の行から並べ、
// 行が足りなければ最後の 1 つが文書の末尾へ新しい行を足す。caret は貼った矩形の左上。
// 矩形の INSERT の Esc（ADR 0035 の追補 #1042）も同じ形で残りの行へ写し、`boundary` が absorb なら
// controller は 1 行目に打った INSERT の単位と 1 つの undo 単位に畳む（貼付は separate）。
struct VimInsertBlock
{
    std::vector<VimBlockEdit> edits;
    Offset caret;
    EditBoundary boundary;
};
} // namespace nenenib::core
