#pragma once

#include <cstdint>

namespace nenenib::core
{
// Ctrl+Tab で歩く向き（ADR 0058 の決定 1）。next は使った順の列で「より前に使った」ほう、previous
// は逆で、端は折り返す。値が増えたら写し先の足りない `switch` がコンパイルで落ちる（CPP-002）。
enum class TabStep : std::uint8_t
{
    next,
    previous
};
} // namespace nenenib::core
