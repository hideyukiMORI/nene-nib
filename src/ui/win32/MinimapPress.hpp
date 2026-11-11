#pragma once

#include "LayoutRect.hpp"
#include "Minimap.hpp"
#include "MinimapGrab.hpp"
#include "MinimapPointing.hpp"
#include "PointMinimap.hpp"

#include <windows.h>

#include <cstdint>
#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
// ミニマップの面の押し・ドラッグ・離し（ADR 0171 の決定 5 の追補）。押したら窓が捕まえ
// （SetCapture・#1464 の教訓）、離すまで点の段を送る。窓の手続きは押しの覚えの次の値と送る意図を
// この 1 本から写す（窓の試験はメッセージの列をここへ流す）。

// 押しの覚えの次の値と、送る意図。
using MinimapStep = std::pair<std::optional<MinimapGrab>, std::optional<application::PointMinimap>>;

// 押しの後の点 y の段（押していなければ覚えも意図も無し）。keep が偽なら覚えを消す（離し）。段が
// 変わったときだけ dragged を送る。
[[nodiscard]] inline MinimapStep
minimap_followed(std::int32_t y, const std::optional<MinimapGrab> &held, bool keep) noexcept
{
    if (!held.has_value())
    {
        return MinimapStep{std::nullopt, std::nullopt};
    }
    const std::int32_t top = held.value().panel_top;
    const std::int32_t row = core::minimap_pointed_row(y, top);
    const auto next = keep ? std::optional{MinimapGrab{top, row}} : std::nullopt;
    if (row == held.value().row)
    {
        return MinimapStep{next, std::nullopt};
    }
    return MinimapStep{next, application::PointMinimap{application::MinimapPointing::dragged, row}};
}

// 押しの点 y と点の下の面（面の外なら値なし）。面の上なら覚えて pressed を送る。
[[nodiscard]] inline MinimapStep
minimap_pressed(std::int32_t y, const std::optional<core::LayoutRect> &panel) noexcept
{
    if (!panel.has_value())
    {
        return MinimapStep{std::nullopt, std::nullopt};
    }
    const std::int32_t top = panel.value().top;
    const std::int32_t row = core::minimap_pointed_row(y, top);
    return MinimapStep{MinimapGrab{top, row},
                       application::PointMinimap{application::MinimapPointing::pressed, row}};
}

// 窓のメッセージ 1 通の後。y は点の y（client の物理画素）、panel は押しの点の下の面（面が出ていて
// 点がその上のときだけ値・WM_LBUTTONDOWN / WM_LBUTTONDBLCLK だけが読む）。
// - WM_LBUTTONDOWN / WM_LBUTTONDBLCLK: 面の上なら覚えて pressed を送る（2 回目の押しも同じ押し）。
//   面の外なら覚えを消し、何も送らない。
// - WM_MOUSEMOVE: 押していれば、段が変わったときだけ dragged を送る（面の外へ出た点も段のまま・
//   切り詰めは application）。
// - WM_LBUTTONUP: 押していれば、段が変わっていれば dragged を送り、覚えを消す。
// - WM_CAPTURECHANGED / WM_MOUSELEAVE / WM_KEYDOWN（Esc のときだけ渡す）: 覚えを消す。窓はその時点
//   の位置のまま（何も送らない）。
// - ほかは変えない（OS のメッセージは開いた集合なので既定分岐を置く・CPP-017）。
[[nodiscard]] inline MinimapStep minimap_step(UINT message, std::int32_t y,
                                              const std::optional<core::LayoutRect> &panel,
                                              const std::optional<MinimapGrab> &held) noexcept
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        return minimap_pressed(y, panel);
    case WM_MOUSEMOVE:
        return minimap_followed(y, held, true);
    case WM_LBUTTONUP:
        return minimap_followed(y, held, false);
    case WM_CAPTURECHANGED:
    case WM_MOUSELEAVE:
    case WM_KEYDOWN:
        return MinimapStep{std::nullopt, std::nullopt};
    default:
        return MinimapStep{held, std::nullopt};
    }
}
} // namespace nenenib::ui::win32
