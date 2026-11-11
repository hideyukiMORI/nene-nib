#pragma once

#include <cstdint>

namespace nenenib::core
{
// 通常モードの語の境を探す向き（ADR 0009 の追補 #1394）。Ctrl+← と Ctrl+Backspace が backward、
// Ctrl+→ と Ctrl+Delete が forward。
enum class OrdinaryWordDirection : std::uint8_t
{
    backward,
    forward
};
} // namespace nenenib::core
