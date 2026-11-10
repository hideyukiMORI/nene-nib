#pragma once

#include <cstdint>

namespace nenenib::core
{
// 再生の種類（ADR 0046 の決定 3・ADR 0176 の追補 33・#1312）。repeat は `.`（記録した鍵は替えた
// 後の鍵なので表を引かない）、macro は `@`（`@@`・`[count]@a`・本文から作ったレジスタも）で、
// controller は鍵を流す時点の `map` の表で引く（Vim の do_execreg は REMAP_YES・start_redo は
// REMAP_NONE）。
enum class VimReplayKind : std::uint8_t
{
    repeat,
    macro
};
} // namespace nenenib::core
