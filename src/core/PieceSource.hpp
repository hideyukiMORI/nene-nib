#pragma once

#include <cstdint>

namespace nenenib::core
{
// piece がどちらのバッファを指すか。original は読んだ本文、add は入力（ADR 0009 の決定 1）。
enum class PieceSource : std::uint8_t
{
    original,
    add
};
} // namespace nenenib::core
