#pragma once

#include <string>

namespace nenenib::core
{
// add バッファの 1 片（ADR 0044 の決定 1）。作るときに容量を確保し、それを超えては伸ばさないので、
// piece が指す string_view は chunk が生きている間ずっと有効である。末尾の chunk を伸ばしてよいのは
// 書き込み済みの長さ（TextBuffer の add_fill_）が bytes.size() と一致する値だけ（決定 2）。
struct AddChunk
{
    std::string bytes;
};
} // namespace nenenib::core
