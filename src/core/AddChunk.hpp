#pragma once

#include "Offset.hpp"

#include <string>
#include <vector>

namespace nenenib::core
{
// add バッファの 1 片（ADR 0044 の決定 1）。作るときに容量を確保し、それを超えては伸ばさないので、
// piece が指す string_view は chunk が生きている間ずっと有効である。末尾の chunk を伸ばしてよいのは
// 書き込み済みの長さ（TextBuffer の add_fill_）が bytes.size() と一致する値だけ（決定 2）。
// newlines は bytes の中の '\n' の位置の昇順で、bytes を伸ばすのと同じ 1 か所で伸びる追記専用の列
// （ADR 0047 の決定 1）。古い値の piece は自分の窓の端までしか読まないので、
// 後から伸びた分は見えない。
struct AddChunk
{
    std::string bytes;
    std::vector<Offset> newlines;
};
} // namespace nenenib::core
