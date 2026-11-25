#pragma once

#include "LineBreakMark.hpp"

#include <d2d1.h>

namespace nenenib::ui::win32
{
// 改行の印を描く所（ADR 0193 の決定 10・#1977）。at
// は行の終わりの位置（最後の画面行の上端・描く先の 座標）、mark は LF か CRLF、height
// は画面行の高さ、space は空白の幅（どちらも物理画素）。
struct LineBreakPlace
{
    D2D1_POINT_2F at;
    core::LineBreakMark mark;
    float height;
    float space;
};
} // namespace nenenib::ui::win32
