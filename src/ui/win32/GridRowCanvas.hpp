#pragma once

#include "CaretShape.hpp"
#include "CompositionView.hpp"
#include "EditorSettings.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaneRole.hpp"
#include "TextViewGeometry.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
// 格子の 1 画面行を描く先と書式（ADR 0140 の決定 5）。area は画面行の本文の領域で、左端が升 0。
// cell は升の幅（BodyTextLayouts::cell が測った物理画素）。寸法は DPI で写した物理画素。
// 色は palette のトークンだけ（ADR 0008）。
struct GridRowCanvas
{
    core::LayoutRect area;
    float cell;
    const core::EditorSettings &settings;
    core::TextViewGeometry geometry;
    const core::Palette &palette;
    core::CaretShape shape;
    // コマンドの入力行が開いているあいだは本文のキャレットを描かない。
    bool caret_shown;
    const std::optional<application::CompositionView> &composition;
    std::int32_t caret_width;
    std::int32_t caret_inset;
    std::int32_t newline_mark;
    float block_radius;
    std::int32_t target_underline;
    std::int32_t other_underline;
    float stroke;
    // 描く枠が操作中か（ADR 0148 の決定 8）。passive ならキャレットは shape
    // の受け身の印（muted）で、 描いた矩形を返さない。
    PaneRole role = PaneRole::active;
};
} // namespace nenenib::ui::win32
