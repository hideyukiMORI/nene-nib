#pragma once

#include <cstddef>

namespace nenenib::core
{
// 本文の位置の手前までの累積（ADR 0140 の決定 4・ADR 0139 の決定 6）。
// column は行頭からの仮想桁の和、cell は格子の升の和（送りの空きを含む・
// text の索引では column と同じ）、code_point は行頭からのコードポイントの数。
// scanned は、この値を出すために文字を読んで歩いた塊の数（0 か 1）。
// plain の塊（改行を含んでもよい）は行頭からの引き算で歩かないので 0。plain でない塊は、
// 読んだのが行の一部だけでも 1 と数える。
// copied はそのとき本文から写したバイト数で、塊の中に行頭があれば
// その行頭から位置までだけを写す。
struct VirtualColumnCursor
{
    std::size_t column;
    std::size_t cell;
    std::size_t scanned;
    std::size_t code_point;
    std::size_t copied;
};
} // namespace nenenib::core
