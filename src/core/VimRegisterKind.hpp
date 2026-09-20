#pragma once

#include <cstdint>

namespace nenenib::core
{
// レジスタに入った本文の種類（ADR 0015 の決定 3 / ADR 0026）。本文だけでは空の文字単位と
// 未使用を区別できない。p が文字の後ろへ貼るか下の行へ貼るかは、この種類が決める。
// 矩形（Ctrl-v）は次の縦切り。
enum class VimRegisterKind : std::uint8_t
{
    uninitialized,
    characters,
    lines
};
} // namespace nenenib::core
