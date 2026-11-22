#pragma once

#include "PaneLayouts.hpp"
#include "SplitGrab.hpp"
#include "SplitLayout.hpp"
#include "SplitPoint.hpp"
#include "SplitShare.hpp"

#include <windows.h>

#include <cstdint>
#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
// 2 枠の境界の押し・ドラッグ・離し・Esc・ダブルクリック（ADR 0148 の 2026-11-22 の追補の決定 3）。
// 窓の手続きは押しの覚え split_grab_ の次の値と送る比（意図 ResizeSplit）をこの 1 本から写す
// （窓の試験 --split-press はメッセージの列をここへ流す・ミニマップの minimap_step と同じ形）。

// 押しの覚えの次の値と、送る比。
using SplitStep = std::pair<std::optional<SplitGrab>, std::optional<core::SplitShare>>;

// 点 (x, y) の分割の向きの座標（上下の分割なら y・ほかは x）。
[[nodiscard]] constexpr std::int32_t split_coordinate_of(core::SplitLayout layout,
                                                         POINT point) noexcept
{
    switch (layout)
    {
    case core::SplitLayout::horizontal:
        return point.y;
    case core::SplitLayout::none:
    case core::SplitLayout::vertical:
        return point.x;
    }
    std::unreachable();
}

// 点の下の見えている境界の始まり（分割の向きの座標）。shown は境界を出している間の枠の配置で、
// Ctrl+P の一覧と全面プレビューの間は値なし（隠れた境界の位置の押しはリンクや選択へ届く・レビュー
// R4 の M1）。境界が隠れているか、点が境界の上でなければ値なし。カーソル・押し・ダブルクリックが
// 同じ 1 本を読む。
[[nodiscard]] inline std::optional<std::int32_t>
split_divider_start_under(const std::optional<core::PaneLayouts> &shown, core::SplitLayout layout,
                          std::uint32_t dpi, POINT point) noexcept
{
    if (!shown.has_value() || !shown.value().divider.has_value() ||
        !core::split_divider_hit(shown.value(), dpi, point.x, point.y))
    {
        return std::nullopt;
    }
    const auto divider = shown.value().divider.value();
    return split_coordinate_of(layout, POINT{divider.left, divider.top});
}

// 押している間の点（押していなければ覚えも比も無し）。押した点から動いたときだけ、境界の始まりを点
// からずれを引いた所に置いた比を、今の比と違えば送る。寸法が無い（1 枠になった）なら比は送らない。
[[nodiscard]] inline SplitStep split_followed(const SplitPoint &point,
                                              const std::optional<SplitGrab> &held) noexcept
{
    if (!held.has_value())
    {
        return SplitStep{std::nullopt, std::nullopt};
    }
    auto grab = held.value();
    if (!grab.dragged && point.coordinate == grab.at)
    {
        return SplitStep{held, std::nullopt};
    }
    grab.dragged = true;
    if (!point.extent.has_value())
    {
        return SplitStep{grab, std::nullopt};
    }
    const auto share = core::split_share_at(point.extent.value(), point.coordinate - grab.offset);
    return SplitStep{grab, share == point.current ? std::nullopt : std::optional{share}};
}

// 境界の上の押し（覚えるだけで送らない）とダブルクリック（等分を送り、覚えない）。境界の外なら
// どちらも覚えを消して何も送らない。
[[nodiscard]] inline SplitStep split_pressed(const SplitPoint &point, bool twice) noexcept
{
    if (!point.start.has_value())
    {
        return SplitStep{std::nullopt, std::nullopt};
    }
    if (twice)
    {
        return SplitStep{std::nullopt, core::SplitShare{}};
    }
    return SplitStep{
        SplitGrab{point.coordinate - point.start.value(), point.coordinate, false, point.current},
        std::nullopt};
}

// 窓のメッセージ 1 通の後。
// - WM_LBUTTONDOWN / WM_LBUTTONDBLCLK: split_pressed。
// - WM_MOUSEMOVE: split_followed。WM_LBUTTONUP: split_followed の比を送り、覚えを消す。
// - WM_KEYDOWN（Esc のときだけ渡す）: 覚えを消し、動かしていれば押す前の比を送る。
// - WM_CAPTURECHANGED: 覚えを消す。比は送った所のまま。
// - ほかは変えない（OS のメッセージは開いた集合なので既定分岐を置く・CPP-017）。
[[nodiscard]] inline SplitStep split_step(UINT message, const SplitPoint &point,
                                          const std::optional<SplitGrab> &held) noexcept
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
        return split_pressed(point, false);
    case WM_LBUTTONDBLCLK:
        return split_pressed(point, true);
    case WM_MOUSEMOVE:
        return split_followed(point, held);
    case WM_LBUTTONUP:
        return SplitStep{std::nullopt, split_followed(point, held).second};
    case WM_KEYDOWN:
        return SplitStep{std::nullopt, held.has_value() && held.value().dragged
                                           ? std::optional{held.value().pressed}
                                           : std::nullopt};
    case WM_CAPTURECHANGED:
        return SplitStep{std::nullopt, std::nullopt};
    default:
        return SplitStep{held, std::nullopt};
    }
}
} // namespace nenenib::ui::win32
