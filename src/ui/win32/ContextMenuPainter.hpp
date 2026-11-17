#pragma once

#include "ContextMenuLayout.hpp"
#include "ContextMenuRow.hpp"
#include "ContextMenuText.hpp"
#include "ContextMenuView.hpp"
#include "ContextMenuWidths.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "PaletteTextRole.hpp"
#include "RgbColor.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <string_view>
#include <utility>

namespace nenenib::ui::win32
{
// 本文の右クリックのメニューを描く経路（ADR 0078 の追補 #1051・設計席の決めの見た目）。面は地
// panel・縁 1 DIP の panel_border・角丸は Ctrl+P の面と同じ 10
// DIP。行は名前を左寄せ（text・使えない 行は
// muted）、鍵の表示を右寄せ（muted）で、選んだ行は面と同じ selection の角丸（6 DIP・行の左右 4 DIP
// の内側）。区切り線は 1 DIP の panel_border。影は付けない。配置は core::context_menu_layout の 1
// 本（窓の当たりと同じ矩形）で、名前と鍵の幅は measure が測る。行の文字組みは PaletteRowLayouts の
// 保持から引く。target は製品では Direct2D の描画先、tests/ui は WIC の bitmap の描画先。色は
// Palette のトークンだけ（ADR 0008）。
class ContextMenuPainter final
{
  public:
    // 保持の上限。7 行の名前と鍵。
    static constexpr std::size_t retained = 16;

    ContextMenuPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                       const ContextMenuText &text, PaletteRowLayouts &layouts) noexcept;
    // 名前と鍵の表示の最も広い幅。測れない行は 0 と数える。
    [[nodiscard]] ContextMenuWidths measure(const application::ContextMenuView &view) const;
    void paint(const application::ContextMenuView &view, const core::ContextMenuLayout &layout,
               const core::Palette &palette);

  private:
    void paint_row(const application::ContextMenuRow &row, const core::ContextMenuLayout &layout,
                   std::size_t index, const core::Palette &palette);
    // 書式と文字組みの焼き方の組。
    using TextStyle = std::pair<IDWriteTextFormat *, PaletteTextRole>;
    void write(std::string_view text, TextStyle style, const core::LayoutRect &area,
               core::RgbColor color);
    [[nodiscard]] std::int32_t text_width(std::string_view text, IDWriteTextFormat *format) const;
    [[nodiscard]] PaletteRowLayouts::TextLayout made(std::string_view text,
                                                     IDWriteTextFormat *format,
                                                     PaletteTextRole role,
                                                     const core::LayoutRect &area) const;
    [[nodiscard]] float scaled(float dips) const noexcept;

    ID2D1RenderTarget &target_;
    ID2D1SolidColorBrush &brush_;
    ContextMenuText text_;
    PaletteRowLayouts &layouts_;
};
} // namespace nenenib::ui::win32
