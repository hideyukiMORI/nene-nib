#pragma once

#include <cstddef>

namespace nenenib::core
{
// 仮想桁の部分和の 1 塊（ADR 0140 の決定 4）。本文の [start, start + bytes) の要約で、
// 塊の始まりの桁（column）・升（cell）・コードポイントの数（code_point）は、その塊が始まる行の
// 行頭から数えた累積（0 始まり）。終わりの値は始まりから O(1) で決まる（VirtualColumnIndex が塊を
// 読まずに次の塊へ足す）:
//
// resets（'\n' を含む） … 終わりの桁は rest、升は tail_cell（最後の '\n' の後ろだけで決まる）
// tab（'\n' を含まず Tab を含む） … 最初の Tab の手前までが lead 桁、Tab は次の 8 の倍数まで、
//                                    その後ろが rest 桁（8 の倍数から数えるので始まりに依らない）
// それ以外 … lead 桁
//
// code_points は終わりの値に効く部分のコードポイントの数（resets なら最後の '\n' の後ろ、それ以外は
// 塊の全体）。line はこの塊より前の '\n' の数、line_breaks は塊の中の '\n' の数で、塊の中の行頭は
// 改行の索引（TextBuffer::line_start）から引く。
//
// 升は桁に、格子の画面行の右端に収まらず次の画面行へ送った空き（padding）を足したもの。padding は
// 始まりの桁の 8 の余りと升の columns の余りだけで決まり、wide（2 升以上の Tab 以外の表示）を
// 含まない塊では 0。plain は表示できる ASCII（U+0020〜U+007E）と '\n' だけの塊で、行の中では
// 桁＝升＝コードポイント＝バイト数（塊の中の桁は改行の索引の行頭からの引き算で、塊を読まない）。
// Tab と制御文字を含む塊は ASCII でも plain でない。
struct VirtualColumnChunk
{
    std::size_t start = 0;
    std::size_t bytes = 0;
    std::size_t column = 0;
    std::size_t cell = 0;
    std::size_t code_point = 0;
    std::size_t lead = 0;
    std::size_t rest = 0;
    std::size_t padding = 0;
    std::size_t tail_cell = 0;
    std::size_t code_points = 0;
    std::size_t line = 0;
    std::size_t line_breaks = 0;
    bool plain = false;
    bool tab = false;
    bool resets = false;
    bool wide = false;
};
} // namespace nenenib::core
