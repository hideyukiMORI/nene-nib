#pragma once

#include <cstdint>

namespace nenenib::core
{
// `Ctrl-a` `Ctrl-x` が読む数の形（ADR 0015 の決定 14）。Nib の `nrformats` は固定の `bin,hex`
// （Vim の defaults.vim の値）で、8 進・英字・unsigned は読まない。
enum class VimNumberForm : std::uint8_t
{
    decimal,
    hexadecimal,
    binary
};
} // namespace nenenib::core
