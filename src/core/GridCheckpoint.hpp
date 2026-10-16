#pragma once

#include <cstddef>

namespace nenenib::core
{
// 格子の 1 行を歩く途中の累積（ADR 0140 の決定 4・試作）。64 KiB の塊の先頭ごとに 1 つ残す。
// byte は行の中のバイト位置、column は code point の位置（0 始まり）、virtual_column は Tab の
// 止まりを決める仮想桁（0 始まり・送りの空きは含まない）、cell は升の位置（0 始まり・送りの空きを
// 含む）。last_start は直前の文字が始まった升（幅 0 の文字はそこへ重ねる）。plain はこの点から
// 次の点までが表示できる ASCII（U+0020〜U+007E）だけで、位置を割り算で出せる塊。
struct GridCheckpoint
{
    std::size_t byte = 0;
    std::size_t column = 0;
    std::size_t virtual_column = 0;
    std::size_t cell = 0;
    std::size_t last_start = 0;
    bool plain = false;
};
} // namespace nenenib::core
