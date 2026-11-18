#pragma once

#include "CompletionFaceLayout.hpp"
#include "CompletionRowView.hpp"
#include "CompletionView.hpp"
#include "ContextMenuText.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "PaletteTextRole.hpp"
#include "RgbColor.hpp"

#include <cstddef>
#include <d2d1.h>
#include <dwrite.h>
#include <string_view>
#include <utility>

namespace nenenib::ui::win32
{
// 補完の面を描く経路（ADR 0169 の決定 4・ADR 0190 の決定 5）。面は地 panel・縁 1 DIP の
// panel_border・角丸は Ctrl+P の面と同じ 10 DIP。行は左から出どころの印（muted の 1 文字・
// core::completion_source_mark）・候補の名前（text）・補足（muted・右寄せ・欄があるときだけ）で、
// 選んだ行は current_line の角丸（6 DIP・行の左右 4 DIP の内側）。影は付けない。配置は
// core::completion_face_layout の 1 本で、行の文字組みは PaletteRowLayouts の保持から引く。書式は
// 右クリックのメニューと同じ組（名前は面の行の名前の書式・印と補足は鍵の表示の書式）を借りる。
// target は製品では Direct2D の描画先、tests/ui は WIC の bitmap の描画先。色は Palette のトークン
// だけで、足していない（ADR 0008）。
class CompletionPainter final
{
  public:
    // 保持の上限。8 行の印・名前・補足と、送ったときの前後の行。
    static constexpr std::size_t retained = 64;

    CompletionPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                      const ContextMenuText &text, PaletteRowLayouts &layouts) noexcept;
    void paint(const application::CompletionView &view, const core::CompletionFaceLayout &layout,
               const core::Palette &palette);

  private:
    void paint_row(const application::CompletionRowView &row,
                   const core::CompletionFaceLayout &layout, std::size_t index,
                   const core::Palette &palette);
    // 書式と文字組みの焼き方の組。
    using TextStyle = std::pair<IDWriteTextFormat *, PaletteTextRole>;
    void write(std::string_view text, TextStyle style, const core::LayoutRect &area,
               core::RgbColor color);
    [[nodiscard]] PaletteRowLayouts::TextLayout made(std::string_view text, TextStyle style,
                                                     const core::LayoutRect &area) const;
    [[nodiscard]] float scaled(float dips) const noexcept;

    ID2D1RenderTarget &target_;
    ID2D1SolidColorBrush &brush_;
    ContextMenuText text_;
    PaletteRowLayouts &layouts_;
};
} // namespace nenenib::ui::win32
