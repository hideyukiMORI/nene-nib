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
// 1 行目だけで、Esc のとき 1 行目が `line_bytes` から伸びたぶんを残りの行へ写す（固定 Vim の
// op_insert / op_change と同じく長さの差で読む）。読み始めは行頭から `at_byte` バイトの所で、
// Vim の textcol と同じ行の中の桁（前の行を消してから改行で 1 行目へ戻っても同じ所・#1173）。
// `column` は写す桁（1 始まり）、`caret` は写したあとのキャレットで、`I` `A` は矩形の左上、
// `c` `C` は空（Esc のふつうの位置）。空かどうかは改行を打ったときの違いも決める（`I` `A` は
// 写さない・`c` `C` は写す・実測）。本文は持たない（ARC-004）。
struct VimBlockInsert
{
    LineNumber first;
    std::size_t lines;
    VimBlockInsertSide side;
    VirtualColumn column;
    std::size_t at_byte;
    std::size_t line_bytes;
    std::optional<Offset> caret;
};
} // namespace nenenib::core
