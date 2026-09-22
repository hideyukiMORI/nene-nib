#pragma once

#include <cstdint>

namespace nenenib::core
{
// レジスタに入った本文の種類（ADR 0015 の決定 3 / ADR 0026 / ADR 0035 の決定 5）。本文だけでは
// 空の文字単位と未使用を区別できない。p が文字の後ろへ貼るか下の行へ貼るか、同じ桁へ何行も
// 貼るかは、この種類が決める。矩形は幅も持つ（VimRegister.width）。
enum class VimRegisterKind : std::uint8_t
{
    uninitialized,
    characters,
    lines,
    block
};
} // namespace nenenib::core
