#pragma once

#include "SelectionAnchoring.hpp"

#include <cstdint>

namespace nenenib::application
{
// 本文の左上からの横位置と画面行。文書位置への変換はapplicationの共通経路。
struct PointText
{
    float x;
    std::int64_t row;
    core::SelectionAnchoring anchoring;
};
} // namespace nenenib::application
