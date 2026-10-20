#pragma once

#include "TextViewGeometry.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// 窓の大きさが変わったときに UI が知らせる「本文が何行入るか」。行数の所有は application。
// window_width / window_height は窓の client の物理画素で、Ctrl+P の面と `:` の補完の面の
// 見えている行数を application が決めるのに読む（ADR 0150 の決定 1）。0 は寸法が届く前で、行数は
// 上限のまま。
struct VisibleLines
{
    std::size_t lines;
    core::TextViewGeometry geometry{};
    std::int32_t window_width = 0;
    std::int32_t window_height = 0;
};
} // namespace nenenib::application
