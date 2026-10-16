#pragma once

#include <cstddef>

namespace nenenib::core
{
// 仮想桁の部分和の 1 塊（ADR 0140 の決定 4）。本文の [start, start + bytes) の要約で、
// 塊の始まりの桁（column）と升（cell）は、その塊が始まる行の行頭から数えた累積（0 始まり）。
// 終わりの桁は始まりの桁から O(1) で決まる（VirtualColumnIndex が塊を読まずに次の塊へ足す）:
//
// resets（'\n' を含む） … 終わりの桁は rest、升は tail_cell（最後の '\n' の後ろだけで決まる）
// tab（'\n' を含まず Tab を含む） … 最初の Tab の手前までが lead 桁、Tab は次の 8 の倍数まで、
//                                    その後ろが rest 桁（8 の倍数から数えるので始まりに依らない）
// それ以外 … lead 桁
//
// 升は桁に、格子の画面行の右端に収まらず次の画面行へ送った空き（padding）を足したもの。padding は
// 始まりの桁の 8 の余りと升の columns の余りだけで決まり、wide（2 升以上の Tab 以外の表示）を
// 含まない塊では 0。plain は表示できる ASCII（U+0020〜U+007E）だけの塊で、桁＝升＝バイト数。
struct VirtualColumnChunk
{
    std::size_t start = 0;
    std::size_t bytes = 0;
    std::size_t column = 0;
    std::size_t cell = 0;
    std::size_t lead = 0;
    std::size_t rest = 0;
    std::size_t padding = 0;
    std::size_t tail_cell = 0;
    bool plain = false;
    bool tab = false;
    bool resets = false;
    bool wide = false;
};
} // namespace nenenib::core
