#pragma once

#include "OffsetRange.hpp"
#include "SyntaxRole.hpp"

namespace nenenib::application
{
// 格子の 1 行の構文の色の区間を本文の位置で持つ（#839・ADR 0156 の追補）。controller が行の
// ColorSpan（本文の桁）を本文の位置へ写した値で、GridLineRows が当たりと同じ写しで画面行の升へ
// 切る。行の中では昇順で重ならない。
struct GridColorRange
{
    core::OffsetRange range;
    core::SyntaxRole role;
};
} // namespace nenenib::application
