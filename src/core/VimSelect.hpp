#pragma once

#include "Selection.hpp"

namespace nenenib::core
{
// VISUAL の選択を置き換える効果（ADR 0018 の決定 3）。VISUAL に入る（anchor = caret）・
// 選択を広げる（anchor はそのまま）・両端を入れ替える（o）が同じ 1 つの形になる。
// 選択の正本は EditorState なので、VimState は anchor を持たない（ARC-004）。
struct VimSelect
{
    Selection selection;
};
} // namespace nenenib::core
