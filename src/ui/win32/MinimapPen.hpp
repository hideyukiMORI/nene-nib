#pragma once

#include "RgbColor.hpp"
#include "SyntaxPalette.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <d2d1.h>

namespace nenenib::ui::win32
{
// ミニマップの色（テーマのトークンだけ・ADR 0171 の決定 2・3）。
// [0] 面の地（background）、[1] 見えている範囲の面（accent）、[2] 区間の無い文字（muted）。
// 構文の役割の色は表に写さず、塊ごとに core::syntax_color（default の無い switch の 1 本）で
// body から引く（役割が増えればそこでコンパイルが落ちる・CPP-002・レビュー M1）。
inline constexpr std::size_t minimap_color_count = 3;
using MinimapColors = std::array<core::RgbColor, minimap_color_count>;

// ミニマップを描く先と単色の brush と色と本文の構文の色（MinimapPicture の関数が受ける 1 組）。
// brush は描く先で作ったもので、色と不透明度は描く関数が置いて戻す。
// 描いている間だけ借りる。dpi は描く先の DPI（見えている範囲の縁の線の太さを物理画素で決める）。
struct MinimapPen
{
    ID2D1RenderTarget &target;
    ID2D1SolidColorBrush &brush;
    const MinimapColors &colors;
    const core::SyntaxPalette &body;
    std::uint32_t dpi = 96;
};
} // namespace nenenib::ui::win32
