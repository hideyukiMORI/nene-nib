#pragma once

#include <cstdint>

namespace nenenib::core
{
// Vim INSERT の code point 移動と通常モードの結合文字を混同しない（ADR 0065）。
enum class CaretUnit : std::uint8_t
{
    code_point,
    ordinary
};
} // namespace nenenib::core
