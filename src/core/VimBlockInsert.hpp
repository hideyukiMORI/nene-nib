#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "VimBlockInsertSide.hpp"
#include "VirtualColumn.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 矩形の `I` `A` `c` `C` で入った INSERT のあいだだけ持つ覚え（ADR 0035 の追補 #1042）。打つのは
// 1 行目だけで、Esc のとき 1 行目が `line_bytes` から伸びたぶん（`at` から）を残りの行へ写す
// （固定 Vim の op_insert / op_change と同じく長さの差で読む）。`column` は写す桁（1 始まり）、
// `caret` は写したあとのキャレットで、`I` `A` は矩形の左上、`c` `C` は空（Esc のふつうの位置）。
// 空かどうかは改行を打ったときの違いも決める（`I` `A` は写さない・`c` `C` は写す・実測）。
// 本文は持たない（ARC-004）。
struct VimBlockInsert
{
    LineNumber first;
    std::size_t lines;
    VimBlockInsertSide side;
    VirtualColumn column;
    Offset at;
    std::size_t line_bytes;
    std::optional<Offset> caret;
};
} // namespace nenenib::core
