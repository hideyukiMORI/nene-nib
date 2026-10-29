#pragma once

#include <cstdint>

namespace nenenib::core
{
// 窓の中に数が見つからなかった理由（ADR 0015 の決定 14）。window_short は窓の端（行の端では
// ない）まで読みに行ったという印で、呼ぶ側が窓を広げて読み直す。
enum class VimNumberMiss : std::uint8_t
{
    absent,
    window_short
};
} // namespace nenenib::core
