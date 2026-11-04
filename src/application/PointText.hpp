#pragma once

#include "PointerClicks.hpp"
#include "SelectionAnchoring.hpp"

#include <cstdint>

namespace nenenib::application
{
// 本文の左上からの横位置と画面行。文書位置への変換はapplicationの共通経路。clicks は続けて押した
// 回数で、2 回目と 3 回目は anchoring を読まずに語と行を選ぶ（#1250）。
struct PointText
{
    float x;
    std::int64_t row;
    core::SelectionAnchoring anchoring;
    PointerClicks clicks;
};
} // namespace nenenib::application
