#pragma once

#include <cstdint>

namespace nenenib::application
{
// まとめて閉じるタブの側（ADR 0078 の追補 #1790）。others は残すタブのほか全部、right は残すタブ
// より帯で右の全部。
enum class TabSweepSide : std::uint8_t
{
    others,
    right
};
} // namespace nenenib::application
