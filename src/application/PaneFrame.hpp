#pragma once

#include "CaretView.hpp"
#include "DisplayForm.hpp"
#include "DocumentView.hpp"
#include "GridRowView.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::application
{
// もう 1 つの枠の表示値（ADR 0148 の決定 8）。lines は見えている行だけで、操作中の枠の lines と
// 同じ関数で作る。caret は受け身の印（選択は無い）。変換中の文字列は載せない。document は ui が
// 将来その枠の文書名を描くための値（今は描かない）。
struct PaneFrame
{
    std::vector<LineView> lines;
    CaretView caret;
    core::LineNumber first_visible;
    std::size_t total_lines;
    float horizontal;
    core::DisplayForm display;
    std::vector<GridRowView> grid;
    DocumentView document;
};
} // namespace nenenib::application
