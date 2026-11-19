#pragma once

#include <cstdint>

namespace nenenib::core
{
// 補完の候補の出どころ（ADR 0169 の決定 5）。面の行の印（本文 / 他のタブ / LSP）を決める。
// LSP の候補（language_server）は L2 が同じ列に足す。
enum class CompletionSource : std::uint8_t
{
    buffer,
    other_buffers,
    language_server
};
} // namespace nenenib::core
