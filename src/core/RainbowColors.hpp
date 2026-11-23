#pragma once

#include "RgbColor.hpp"

#include <array>
#include <cstddef>

namespace nenenib::core
{
// 虹の色の巡回（ADR 0193 の決定 5）。colors の先頭 count 色だけが有効で、段 i（括弧の深さ i）は
// colors[i % count] を使う。count は 0〜6（0 はテーマから 1 色も取れなかったとき）。どの欄も単独で
// 妥当な値なので公開 aggregate（CPP-003）。
struct RainbowColors
{
    std::array<RgbColor, 6> colors;
    std::size_t count;
};
} // namespace nenenib::core
