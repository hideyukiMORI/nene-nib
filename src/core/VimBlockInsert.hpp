#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "VimBlockInsertSide.hpp"
#include "VirtualColumn.hpp"

#include <cstddef>
#include <memory>
#include <optional>

namespace nenenib::core
{
// 矩形の `I` `A` `c` `C` で入った INSERT のあいだだけ持つ覚え（ADR 0035 の追補 #1042）。打つのは
// 1 行目だけで、Esc のとき 1 行目が `line_bytes` から伸びたぶんを残りの行へ写す（固定 Vim の
// op_insert / op_change と同じく長さの差で読む）。読み始めは行頭から `at_byte` バイトの所で、
// Vim の textcol と同じ行の中の桁（前の行を消してから改行で 1 行目へ戻っても同じ所・#1173）。
// `column` は写す桁（1 始まり）、`caret` は写したあとのキャレットで、`I` `A` は矩形の左上、
// `c` `C` は空（Esc のふつうの位置）。空かどうかは改行を打ったときの違いも決める（`I` `A` は
// 写さない・`c` `C` は写す・実測）。
// `carried` は `A` の 1 行目で右の端が切った 1 バイトの文字（Tab か制御文字）の数（0 か 1）。
// Esc は `at_byte` の手前のその文字を打った本文の頭に付けて写す（#1189）。決めるのは入口の 1 回。
// `straddled` は `I` の 1 行目で左の端が切った文字（幅 2 以上の文字・Tab・2 桁以上の制御文字や
// 書式用文字）のうち端より前の桁の数（掛からなければ 0）。1 行目はその文字の頭から打ち、Esc は
// 打った本文のうち端の桁から後ろだけを写す（#1196・block_copied_body）。決めるのは入口の 1 回。
// `left` は矩形の左の桁（1 始まり）。INSERT の中でキャレットを動かしてから打つと、
// 覚えは打ち始めへ置き直す（#1687・置き直すのは block_insert_moved の 1 か所）。
// `entry` は置き直した覚えが持つ入口の覚えの写し（入口のままなら空）。
// 写さないと決めた覚えは `lines` が 1（1 行目だけ）。
// `typed_run` は `I` で打った後に打ち始めより手前へ動いて打ったとき、動く前に打った
// 本文のバイト数（Esc はそれに収まる本文だけを写す）。ほかは空。
// 単位: `at_byte` `line_bytes` `carried` `typed_run` はバイト、
// `column` `left` `straddled` は仮想桁。
// 本文は持たない（ARC-004）。
struct VimBlockInsert
{
    LineNumber first;
    std::size_t lines;
    VimBlockInsertSide side;
    VirtualColumn column;
    std::size_t at_byte;
    std::size_t line_bytes;
    std::optional<Offset> caret;
    std::size_t carried;
    std::size_t straddled;
    VirtualColumn left;
    std::shared_ptr<const VimBlockInsert> entry;
    std::optional<std::size_t> typed_run;
};
} // namespace nenenib::core
