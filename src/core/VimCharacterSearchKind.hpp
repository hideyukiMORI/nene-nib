#pragma once

#include <cstdint>

namespace nenenib::core
{
// f / F / t / T の閉じた4種。方向と対象文字上へ載るかをこの値だけで表す（ADR 0026）。
enum class VimCharacterSearchKind : std::uint8_t
{
    find_forward,
    find_backward,
    till_forward,
    till_backward
};
} // namespace nenenib::core
