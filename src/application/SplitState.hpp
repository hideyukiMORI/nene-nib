#pragma once

#include "OtherView.hpp"
#include "PaneSide.hpp"
#include "SplitLayout.hpp"
#include "SplitShare.hpp"

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
    // 境界の位置（ADR 0148 の 2026-11-22 の追補の決定 1）。置くのは with_split_share だけで、分け
    // る・閉じるは等分（既定値）に戻る。本文の 2 枠から全面プレビューへ出入りしても比は保つ（隠した
    // 枠と一緒に split を残す）。横のプレビューから全面へ入る道は split を空にして配置と側と比を
    // 覚え、側へ戻るときに置き直す（#1973）。layout == none なら読まない。
    core::SplitShare share{};
};

[[nodiscard]] inline bool operator==(const SplitState &left, const SplitState &right) noexcept
{
    return left.layout == right.layout && left.active_side == right.active_side &&
           left.other == right.other && left.share == right.share;
}
} // namespace nenenib::application
