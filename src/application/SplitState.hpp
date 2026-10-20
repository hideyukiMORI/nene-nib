#pragma once

#include "OtherView.hpp"
#include "PaneSide.hpp"
#include "SplitLayout.hpp"

#include <optional>

namespace nenenib::application
{
// 窓の枠の状態（ADR 0148 の決定 2）。active_side は操作中の枠が画面のどちら側か（first は左か上）。
// 不変条件は layout == none ⇔ other が値なし、と layout == none なら active_side == first で、
// 揃えて置くのは EditorState の with_split（first）/ with_pane_swapped（反転）/ without_split
// （first に戻す）/ with_preview（1 枠なら vertical・first）だけ。1 枠は SplitState{}
// で、今までの経路そのもの。 other は受け身の本文の枠かプレビューの閉じた和型（ADR 0155 の決定
// 1）。
struct SplitState
{
    core::SplitLayout layout = core::SplitLayout::none;
    core::PaneSide active_side = core::PaneSide::first;
    std::optional<OtherView> other;
};

[[nodiscard]] inline bool operator==(const SplitState &left, const SplitState &right) noexcept
{
    return left.layout == right.layout && left.active_side == right.active_side &&
           left.other == right.other;
}
} // namespace nenenib::application
