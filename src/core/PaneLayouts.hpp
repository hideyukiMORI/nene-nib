#pragma once

#include "BodyLayout.hpp"
#include "FontSize.hpp"
#include "LayoutRect.hpp"
#include "PaneSide.hpp"
#include "SplitLayout.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::core
{
// 本文の枠の矩形（ADR 0148 の決定 1・8）。位置の順で、first は左か上の枠、second は右か下の枠、
// divider は 2 つの枠の境界。どちらが操作中かは SplitState::active_side（frame の active_side）で
// 引く。1 枠では second と divider は値なしで、first は body_layout と同じ値。どの欄も単独で
// 妥当な値なので公開 aggregate（CPP-003）。
struct PaneLayouts
{
    BodyLayout first;
    std::optional<BodyLayout> second;
    std::optional<LayoutRect> divider;
};

// 本文の帯を配置 layout で等分する。左右の等分は (帯の幅 − 境界) / 2 を切り捨てて両枠に同じ幅を
// 与え、余りの画素は境界に足す（境界は 1 DIP を物理画素に丸めて 1 以上）。上下は高さを同じ規則で
// 分ける。各枠の行番号の欄・本文・見えている行数は body_layout と同じ規則で枠の帯から作る。
// client は窓の client 領域（左上が原点）で、1 枠の値は body_layout(client の幅, 高さ, dpi,
// font_size) と同じ。
[[nodiscard]] PaneLayouts pane_layouts(LayoutRect client, std::uint32_t dpi, FontSize font_size,
                                       SplitLayout layout) noexcept;

// 操作中の枠の配置（ADR 0148 の決定 8）。active はその枠の側（frame の active_side）。1 枠
// （second が値なし）では側を問わず first。描画・窓の行数・マウスが同じ 1 本で引く（ARC-001）。
[[nodiscard]] BodyLayout active_pane_layout(const PaneLayouts &layouts, PaneSide active) noexcept;

// もう 1 つの枠の配置。active の反対の側で、1 枠なら値なし。
[[nodiscard]] std::optional<BodyLayout> other_pane_layout(const PaneLayouts &layouts,
                                                          PaneSide active) noexcept;
} // namespace nenenib::core
