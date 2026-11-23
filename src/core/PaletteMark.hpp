#pragma once

#include "PaletteMarkHint.hpp"
#include "PaletteScope.hpp"
#include "UiMessage.hpp"

namespace nenenib::core
{
// 出どころの記号の表の 1 行（ADR 0060 の決定 2・CPP-012）。mark は入力の先頭の 1 文字、name は
// 検索欄の案内に出す名前（文の表の名前・#1019）、hint は案内に出すか（ADR 0060 の 2026-11-22 の
// 追補）。
struct PaletteMark
{
    char mark;
    PaletteScope scope;
    UiMessage name;
    PaletteMarkHint hint;
};
} // namespace nenenib::core
