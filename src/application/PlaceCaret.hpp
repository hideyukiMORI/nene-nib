#pragma once

#include "SelectionAnchoring.hpp"
#include "TextPosition.hpp"

namespace nenenib::application
{
// 本文のクリックでキャレットを置く。行と桁は UI が当たり判定で求めて渡す（ARC-011）。
// Shift を押しながらなら anchor を固定して選択を伸ばす（ADR 0009 の決定 4）。
struct PlaceCaret
{
    core::TextPosition position;
    core::SelectionAnchoring anchoring;
};
} // namespace nenenib::application
