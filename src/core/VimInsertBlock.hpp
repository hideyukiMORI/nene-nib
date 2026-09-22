#pragma once

#include "Offset.hpp"
#include "VimBlockEdit.hpp"

#include <vector>

namespace nenenib::core
{
// 矩形レジスタの貼付（NORMAL の `p` `P`・ADR 0035 の決定 6）。行ごとの挿入を上の行から並べ、
// 行が足りなければ最後の 1 つが文書の末尾へ新しい行を足す。caret は貼った矩形の左上。
struct VimInsertBlock
{
    std::vector<VimBlockEdit> edits;
    Offset caret;
};
} // namespace nenenib::core
