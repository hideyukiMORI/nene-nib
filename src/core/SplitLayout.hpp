#pragma once

#include <cstdint>

namespace nenenib::core
{
// 本文の枠の配置（ADR 0148 の決定 1）。枠は最大 2 つで、左右（vertical）か上下（horizontal）の
// 等分。none は 1 枠で、今までの経路そのもの。値を足したら落ちた switch を直す（CPP-002）。
enum class SplitLayout : std::uint8_t
{
    none,
    vertical,
    horizontal
};
} // namespace nenenib::core
