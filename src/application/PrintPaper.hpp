#pragma once

#include <cstdint>

namespace nenenib::application
{
// 印刷の 1 枚の紙の大きさ（ADR 0177 の決定 5・10）。ui の印刷のダイアログが決めて PrintSettings
// で渡す。単位は Direct2D の DIP（1/96 インチ）で、印刷の文字組みはこの単位を物理画素として
// dpi 96（core::reference_dpi）で組む。向きは入れ替え済み。
struct PrintPaper
{
    std::int32_t width;
    std::int32_t height;
};
} // namespace nenenib::application
