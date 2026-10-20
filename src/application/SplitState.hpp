#pragma once

#include "PaneSide.hpp"
#include "PaneView.hpp"
#include "SplitLayout.hpp"

#include <optional>

namespace nenenib::application
{
// 窓の枠の状態（ADR 0148 の決定 2）。active_side は操作中の枠が画面のどちら側か（first は左か上）。
// 不変条件は layout == none ⇔ other が値なし、と layout == none なら active_side == first で、
// 揃えて置くのは EditorState の with_split（first）/ with_pane_swapped（反転）/ without_split
// （first に戻す）だけ。1 枠は SplitState{} で、今までの経路そのもの。
struct SplitState
{
    core::SplitLayout layout = core::SplitLayout::none;
    core::PaneSide active_side = core::PaneSide::first;
    std::optional<PaneView> other;
};

[[nodiscard]] constexpr bool operator==(const SplitState &left, const SplitState &right) noexcept
{
    return left.layout == right.layout && left.active_side == right.active_side &&
           left.other == right.other;
}
} // namespace nenenib::application
