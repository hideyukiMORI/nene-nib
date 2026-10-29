#pragma once

#include "ExplorerLayout.hpp"
#include "ExplorerRowView.hpp"
#include "ExplorerText.hpp"
#include "ExplorerView.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "RgbColor.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <string_view>

namespace nenenib::ui::win32
{
// エクスプローラーの面を描く経路（ADR 0162 の決定 2・8・採用案
// docs/design/2026-10-28-explorer-look.md）。地は panel・本文との境界（面の右端の border）は
// panel_border で焦点があれば accent・選んだ行は current_line で行の全幅・名前は text・隠しと
// 印 ▸ ▾ とルート名は muted。先頭の行はルート名（ルートが無ければ案内）、その下に view.rows を
// 1 行ずつ。行の横は「余白 + 深さ × 字下げ」に印の升、その右に名前で、長い名前は書式に焼いた
// 文字単位の省略記号「…」で切る（タブの題名と同じ規則）。行の文字組みは PaletteRowLayouts の
// 保持から引き、同じ行は frame をまたいで作り直さない（#904 の形）。target は製品では Direct2D の
// 描画先、tests/ui は WIC の bitmap の描画先。色は Palette のトークンだけ（ADR 0008）。
class ExplorerPainter final
{
  public:
    // 面の行の文字組みの保持の上限。名前は見えている行の数、印は 2 種類、ルートは 1 つ。
    static constexpr std::size_t retained = 512;

    ExplorerPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                    const ExplorerText &text, PaletteRowLayouts &layouts) noexcept;
    // 面（layout.panel）を描く。面が無ければ何もしない。
    void paint(const application::ExplorerView &view, const core::ExplorerLayout &layout,
               const core::Palette &palette);

  private:
    void fill(const core::LayoutRect &area, core::RgbColor color);
    void write(std::string_view text, const core::LayoutRect &area, core::RgbColor color);
    void paint_root(const application::ExplorerView &view, const core::LayoutRect &line,
                    const core::ExplorerLayout &layout, const core::Palette &palette);
    void paint_row(const application::ExplorerRowView &row, const core::LayoutRect &line,
                   const core::ExplorerLayout &layout, const core::Palette &palette);
    [[nodiscard]] PaletteRowLayouts::TextLayout made(std::string_view text,
                                                     const core::LayoutRect &area);

    ID2D1RenderTarget &target_;
    ID2D1SolidColorBrush &brush_;
    ExplorerText text_;
    PaletteRowLayouts &layouts_;
};
} // namespace nenenib::ui::win32
