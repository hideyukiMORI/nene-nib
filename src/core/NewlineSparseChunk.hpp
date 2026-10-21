#pragma once

#include <cstdint>
#include <vector>

namespace nenenib::core
{
// 改行の索引の 64 KiB の塊のうち、LF が 4,096 個未満の塊の表現（ADR 0121 の追補の決定 3）。
// positions は塊の先頭からの LF の位置の昇順の列（2 byte × 個数 ≤ 8 KiB）。
struct NewlineSparseChunk
{
    std::vector<std::uint16_t> positions;
};
} // namespace nenenib::core
