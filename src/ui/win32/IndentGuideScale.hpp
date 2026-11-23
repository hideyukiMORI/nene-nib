#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// インデントガイドの寸法の元（ADR 0193 の決定 9）。space は本文の書式の空白の幅
// （BodyTextLayouts::space_width・物理画素）、dpi は線の太さを物理画素にする DPI。
// indent_guide_row に渡す束で、公開 aggregate（CPP-003）。
struct IndentGuideScale
{
    float space;
    std::uint32_t dpi;
};
} // namespace nenenib::ui::win32
