#pragma once

#include "InvisibleKind.hpp"

#include <dwrite.h>

namespace nenenib::ui::win32
{
// 印を描く 1 字（ADR 0193 の決定 10・#1977）。unit は描いた layout の UTF-16 の位置（空白・Tab・
// 全角空白はどれも 1 単位）、kind は印の形と色。前から順に並べる。
struct InvisibleMark
{
    UINT32 unit;
    core::InvisibleKind kind;
};
} // namespace nenenib::ui::win32
