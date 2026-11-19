#pragma once

#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 補完の面の配置（ADR 0169 の決定 4）。panel は面の矩形、first は描く最初の行（表示値の行の中の
// 位置）、rows は描く行の数、above はキャレットの上に出したこと、detailed は補足の欄があること。
// 行の矩形は completion_face_row が panel から引く。
struct CompletionFaceLayout
{
    LayoutRect panel;
    std::size_t first;
    std::size_t rows;
    bool above;
    bool detailed;
    std::uint32_t dpi;
};
} // namespace nenenib::core
