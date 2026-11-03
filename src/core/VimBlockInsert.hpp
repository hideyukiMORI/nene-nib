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
// `carried` は `A` の 1 行目で右の端が切った 1 バイトの文字（Tab か制御文字）の数（0 か 1）。
// Esc は `at` の手前のその文字を打った本文の頭に付けて写す（#1189）。決めるのは入口の 1 回。
// `straddled` は `I` の 1 行目で左の端が切った文字（幅 2 以上の文字・Tab・2 桁以上の制御文字や
// 書式用文字）のうち端より前の桁の数（掛からなければ 0）。1 行目はその文字の頭から打ち、Esc は
// 打った本文のうち端の桁から後ろだけを写す（#1196・block_copied_body）。決めるのは入口の 1 回。
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
    std::size_t carried;
    std::size_t straddled;
};
} // namespace nenenib::core
