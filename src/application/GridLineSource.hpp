#pragma once

#include "EditorState.hpp"
#include "GridCells.hpp"
#include "GridLineMarks.hpp"
#include "GridPoint.hpp"
#include "LineNumber.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// 1 行の見えている画面行 [first_row, end_row) を作る入力（ADR 0140 の決定 5）。frame を作る
// 同期呼出しの間だけ状態と升の答え手（本文の索引を借りた GridCells・frame に 1 つ）を借用する。
// caret はキャレットが立つ画面行と升（VisualTextView::grid_point と同じ升）で、使うのは
// キャレットがこの行にあるときだけ。
struct GridLineSource
{
    const EditorState &state;
    GridCells &cells;
    core::LineNumber line;
    std::size_t first_row;
    std::size_t end_row;
    std::int64_t screen_row;
    GridLineMarks marks;
    core::GridPoint caret;
};
} // namespace nenenib::application
