#pragma once

#include "Offset.hpp"
#include "PieceSource.hpp"

#include <cstddef>

namespace nenenib::core
{
// piece table の 1 片。どちらのバッファの、どこから、何バイトか（ADR 0009 の決定 1）。
// add の piece は 1 つの chunk の中だけを指し、chunk はその番号・start は chunk の中の位置である。
// original の piece は chunk を使わず 0 を持つ（ADR 0044 の決定 1）。
// 改行の索引はバッファごとに 1 本で、piece はそのうち自分の範囲に入る窓 [newline_begin,
// newline_end) の添字だけを持つ（ADR 0047 の決定 2）。窓の幅がその piece の改行の数である。
// prefix_bytes と prefix_newlines は列の中でこの piece の手前までの本文のバイト数と改行の数で、
// 列の末尾に置く 1 か所（TextBuffer.cpp の placed）だけが、列を作る周の中で埋める。読む口は
// これを二分探索で引く（ADR 0183 の決定 2・3）。列の外で作る piece の値は 0 で、列に置くときに
// 埋め直される。
struct Piece
{
    PieceSource source;
    std::size_t chunk;
    Offset start;
    std::size_t length;
    std::size_t newline_begin;
    std::size_t newline_end;
    std::size_t prefix_bytes;
    std::size_t prefix_newlines;
};
} // namespace nenenib::core
