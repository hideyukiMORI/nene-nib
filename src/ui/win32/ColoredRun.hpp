#pragma once
#include <d2d1.h>
#include <dwrite.h>

namespace nenenib::ui::win32
{
// 構文の色を塗る本文の 1 区間（ADR 0156 の決定 11）。range は描く layout の UTF-16 の範囲で、
// 行の中では前から順に重ならない。color は core::syntax_color の答えを写した色。
struct ColoredRun
{
    DWRITE_TEXT_RANGE range;
    D2D1_COLOR_F color;
};
} // namespace nenenib::ui::win32
