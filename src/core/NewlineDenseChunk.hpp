#pragma once

#include <cstdint>
#include <vector>

namespace nenenib::core
{
// 改行の索引の 64 KiB の塊のうち、LF が 4,096 個以上の塊の表現（ADR 0121 の追補の決定 4）。
// words は 1 byte に 1 bit の 1,024 word（LF の位置だけ立つ・書いていない所は 0）。counts は
// 512 byte の区切りごとに「その区切りの前まで」の塊の中の LF の数（128 個・最大 65,024 なので
// 16 bit に収まる）。書きかけの末尾の塊では、書いたバイトの区切りまでだけが正しい値を持つ。
struct NewlineDenseChunk
{
    std::vector<std::uint64_t> words;
    std::vector<std::uint16_t> counts;
};
} // namespace nenenib::core
