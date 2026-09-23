#pragma once

#include "Offset.hpp"
#include "PieceSource.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::core
{
// piece table の 1 片。どちらのバッファの、どこから、何バイトか（ADR 0009 の決定 1）。
// newlines は piece の先頭からの相対位置で、'\n' のバイトそのものを指す。
// CRLF は '\r\n' で 1 つの改行なので、'\n' だけを数えれば piece の境界に跨っても数が狂わない。
// add の piece は 1 つの chunk の中だけを指し、chunk はその番号・start は chunk の中の位置である。
// original の piece は chunk を使わず 0 を持つ（ADR 0044 の決定 1）。
struct Piece
{
    PieceSource source;
    std::size_t chunk;
    Offset start;
    std::size_t length;
    std::vector<Offset> newlines;
};
} // namespace nenenib::core
