#pragma once

#include "RgbColor.hpp"

#include <array>
#include <cstddef>
#include <d2d1.h>

namespace nenenib::ui::win32
{
// ミニマップの色（テーマのトークンだけ・ADR 0171 の決定 2・3）。
// [0] 面の地（background）、[1] 見えている範囲の面（accent）、
// [2] 区間の無い文字（muted）、[3] から core::SyntaxRole の順の構文の色。
inline constexpr std::size_t minimap_color_count = 13;
using MinimapColors = std::array<core::RgbColor, minimap_color_count>;

// ミニマップを描く先と単色の brush と色（MinimapPicture の関数が受ける 1 組）。
// brush は描く先で作ったもので、色と不透明度は描く関数が置いて戻す。
// 描いている間だけ借りる。
struct MinimapPen
{
    ID2D1RenderTarget &target;
    ID2D1SolidColorBrush &brush;
    const MinimapColors &colors;
};
} // namespace nenenib::ui::win32
