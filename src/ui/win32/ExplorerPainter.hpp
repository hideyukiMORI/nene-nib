#pragma once

#include "ExplorerLayout.hpp"
#include "ExplorerRowView.hpp"
#include "ExplorerText.hpp"
#include "ExplorerView.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "RgbColor.hpp"
#include "SyntaxPalette.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <span>
#include <string_view>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// エクスプローラーの面を描く経路（ADR 0162 の決定 2・8・採用案 B
// docs/design/2026-11-10-explorer-look.md・施主決定 D81）。地はタブの帯と同じ title_bar
// で、帯と面が L 字の枠になる。面の右端の border は panel_border で焦点があれば
// accent、本文の帯の左上の角は 半径 corner で丸め（角の外は
// title_bar・角の弧も境界の色）。先頭の行はルート名（text・semibold・ ルートが無ければ案内を
// muted）、その下に view.rows を 1 行ずつ。行の部品の矩形は core の explorer_row_parts の 1
// 本で、選んだ行は background の丸い面（焦点があれば accent の 1 DIP の枠）、 載せた行は tab_hover
// の丸い面（選んだ行には重ねない）、字下げの縦の線は panel_border、山形と フォルダの絵は muted
// の線（1.25 DIP）、ファイルの印は 6 DIP の角丸の四角で色は行の mark の構文の 色（値なしは
// muted）、名前は text（隠しは muted・アクティブな文書は semibold）、未保存は accent
// の丸。長い名前は書式に焼いた文字単位の省略記号「…」で切る（タブの題名と同じ規則）。行の文字組みは
// PaletteRowLayouts の保持から鍵の hash で O(1) に引き（#1125）、同じ行は frame をまたいで作り直さ
// ない（#904 の形）。線の形（stroke style）と角の形（path geometry）は描画先の factory から 1 frame
// に 1 回だけ作る。target は製品では Direct2D の描画先、tests/ui は WIC の bitmap の描画先。色は
// Palette と SyntaxPalette のトークンだけ（ADR 0008）。
class ExplorerPainter final
{
  public:
    // 面の行の文字組みの保持の上限。名前は見えている行の数（書式 2 つ）、ルートは 1 つ。
    static constexpr std::size_t retained = 512;

    ExplorerPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                    const ExplorerText &text, PaletteRowLayouts &layouts) noexcept;
    // 面（layout.panel）を描く。面が無ければ何もしない（本文の角も丸めない）。
    void paint(const application::ExplorerView &view, const core::ExplorerLayout &layout,
               const core::Palette &palette, const core::SyntaxPalette &syntax);

  private:
    void fill(const core::LayoutRect &area, core::RgbColor color);
    void fill_round(const core::LayoutRect &area, float radius, core::RgbColor color);
    void write(std::string_view text, const core::LayoutRect &area, core::RgbColor color,
               IDWriteTextFormat *format);
    void paint_root(const application::ExplorerView &view, const core::ExplorerLayout &layout);
    void paint_row(const application::ExplorerView &view, std::size_t index,
                   const core::ExplorerLayout &layout);
    void paint_surface(const application::ExplorerView &view, std::size_t index,
                       const core::LayoutRect &surface, const core::ExplorerLayout &layout);
    void paint_lead(const application::ExplorerRowView &row, const core::LayoutRect &chevron,
                    const core::LayoutRect &mark);
    void stroke_lines(const core::LayoutRect &cell, std::span<const D2D1_POINT_2F> points,
                      bool closed, core::RgbColor color);
    void paint_corner(const core::ExplorerLayout &layout, core::RgbColor edge,
                      core::RgbColor ground);
    [[nodiscard]] float scale() const noexcept;
    [[nodiscard]] PaletteRowLayouts::TextLayout
    made(std::string_view text, const core::LayoutRect &area, IDWriteTextFormat *format);

    ID2D1RenderTarget &target_;
    ID2D1SolidColorBrush &brush_;
    ExplorerText text_;
    PaletteRowLayouts &layouts_;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> stroke_;
    // paint が受けた色（1 回の描画の間だけ・行ごとの関数へ引数で回さない）。
    core::Palette palette_{};
    core::SyntaxPalette syntax_{};
};
} // namespace nenenib::ui::win32
