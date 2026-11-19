#pragma once

#include "LayoutRect.hpp"

#include <cstdint>

namespace nenenib::ui::win32
{
// 札の置き場の手がかり（施主決定 D106・#1869 の段 L2）。link はクリックしたリンクを囲む矩形
// （PreviewPane::range_box・物理画素）、area は札を収める領域、dpi は寸法の倍率。公開 aggregate
// （CPP-003）。
struct LinkCardPlace
{
    core::LayoutRect link;
    core::LayoutRect area;
    std::uint32_t dpi;
};
} // namespace nenenib::ui::win32
