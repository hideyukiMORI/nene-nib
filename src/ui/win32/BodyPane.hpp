#pragma once

#include "BodyLayout.hpp"
#include "CaretView.hpp"
#include "GridRowView.hpp"
#include "LineView.hpp"
#include "PaneRole.hpp"

#include <span>

namespace nenenib::ui::win32
{
// 1 つの枠の本文を描く束（ADR 0148 の決定 8）。操作中の枠は EditorFrame の lines・grid・caret、
// もう 1 つの枠は PaneFrame の同じ欄を借りる。body はその枠の配置（pane_layouts の片側）。
// 両方の枠が同じ draw_line 群と draw_grid_rows を通るための借用で、描く間だけ使う。
struct BodyPane
{
    const core::BodyLayout &body;
    std::span<const application::LineView> lines;
    std::span<const application::GridRowView> grid;
    const application::CaretView &caret;
    PaneRole role;
};
} // namespace nenenib::ui::win32
