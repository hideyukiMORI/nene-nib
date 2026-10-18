#pragma once

#include "PaneView.hpp"
#include "SplitLayout.hpp"

#include <optional>

namespace nenenib::application
{
// 窓の枠の状態（ADR 0148 の決定 2）。不変条件は layout == none ⇔ other が値なしで、両方を揃えて
// 置くのは EditorState の with_split / with_pane_swapped / without_split だけ。1 枠は
// SplitState{} で、今までの経路そのもの。
struct SplitState
{
    core::SplitLayout layout = core::SplitLayout::none;
    std::optional<PaneView> other;
};

[[nodiscard]] constexpr bool operator==(const SplitState &left, const SplitState &right) noexcept
{
    return left.layout == right.layout && left.other == right.other;
}
} // namespace nenenib::application
