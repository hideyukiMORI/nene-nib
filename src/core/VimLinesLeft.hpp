#pragma once

#include <cstdint>

namespace nenenib::core
{
// 鍵の後に行が残るか（ADR 0188 の決定 3 (a)）。none は Vim が最後の行まで消して ML_EMPTY にした鍵
// （`ggdG` `:%d` `VGd`・1 行での `dd`）。engine が出口で立て、controller は編集の中身から推し量ら
// ない。値が増えたら switch が落ちる（CPP-002）。
enum class VimLinesLeft : std::uint8_t
{
    some,
    none
};
} // namespace nenenib::core
