#pragma once

#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "PaletteTextRole.hpp"
#include "RgbColor.hpp"
#include "StartEntry.hpp"
#include "StartRowLayout.hpp"
#include "StartScreenLayout.hpp"
#include "StartScreenText.hpp"
#include "StartScreenView.hpp"

#include <cstddef>
#include <d2d1.h>
#include <dwrite.h>
#include <string_view>
#include <utility>

namespace nenenib::ui::win32
{
// 起動画面を描く経路（ADR 0172 の決定 5・採用案 docs/design/2026-10-28-start-screen-look.md）。
// 本文の面（本文の地と行番号の上）の中央寄せの 1 列に、題「NeNe Nib」・節の見出し（muted）・
// 項目（text・パスは末尾を「…」で切る）・選んだ行（current_line で列の全幅・先頭に ▸ を muted）・
// 「はじめる」の右端の補足（muted・ファイルを開くは鍵の枠）・鍵の案内（鍵の枠と muted の説明）を
// 描く。配置は core::start_screen_layout の 1 本（窓のクリックの当たりと同じ）。行の文字組みは
// PaletteRowLayouts の保持から引き、同じ画面は frame をまたいで作り直さない（#904 の形）。
// target は製品では Direct2D の描画先、tests/ui は WIC の bitmap の描画先。色は Palette の
// トークンだけ（ADR 0008）。
class StartScreenPainter final
{
  public:
    // 保持の上限。題 1・見出し 4・空の知らせ 1・行 23（項目と補足）・鍵 3 組で 60 に収まる。
    static constexpr std::size_t retained = 64;

    StartScreenPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                       const StartScreenText &text, PaletteRowLayouts &layouts) noexcept;
    // 画面を描く。clip は本文の帯（列が帯の下へはみ出す分を切る）。
    void paint(const application::StartScreenView &view, const core::StartScreenLayout &layout,
               const core::Palette &palette, const core::LayoutRect &clip);

  private:
    void fill(const core::LayoutRect &area, core::RgbColor color);
    // 書式と文字組みの焼き方の組。
    using TextStyle = std::pair<IDWriteTextFormat *, PaletteTextRole>;
    void write(std::string_view text, TextStyle style, const core::LayoutRect &area,
               core::RgbColor color);
    void keycap(std::string_view key, const core::LayoutRect &area);
    void paint_headings(const core::StartScreenLayout &layout, const core::Palette &palette);
    void paint_row(const core::StartEntry &entry, const core::StartRowLayout &row, bool selected,
                   const core::Palette &palette);
    void paint_keys(const application::StartScreenView &view, const core::StartScreenLayout &layout,
                    const core::Palette &palette);
    [[nodiscard]] PaletteRowLayouts::TextLayout made(std::string_view text, TextStyle style,
                                                     const core::LayoutRect &area);

    ID2D1RenderTarget &target_;
    ID2D1SolidColorBrush &brush_;
    StartScreenText text_;
    PaletteRowLayouts &layouts_;
};
} // namespace nenenib::ui::win32
