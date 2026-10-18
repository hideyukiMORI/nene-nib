#pragma once

#include <cstdint>

namespace nenenib::core
{
// 縦の位置合わせでキャレットの行（格子では画面行）を置く所（ADR 0146 の決定 2・5）。
// 画面の上・中・下。
enum class VimScreenPlace : std::uint8_t
{
    top,
    center,
    bottom
};
} // namespace nenenib::core
