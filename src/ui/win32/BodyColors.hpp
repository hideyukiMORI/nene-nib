#pragma once
#include "ColoredRun.hpp"

#include <d2d1.h>
#include <span>

namespace nenenib::ui::win32
{
// 1 行の本文の色（ADR 0156 の決定 11）。runs は構文の色の区間（描く layout の UTF-16 の範囲で
// 前から順）、plain は区間の外の字の色（text）。描く 1 回の間だけ区間の列を借りる。
struct BodyColors
{
    std::span<const ColoredRun> runs;
    D2D1_COLOR_F plain;
};
} // namespace nenenib::ui::win32
