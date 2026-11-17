#pragma once

#include "FinderPreview.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteLayout.hpp"
#include "PaletteRowLayouts.hpp"
#include "PreviewLine.hpp"
#include "PreviewPaneText.hpp"
#include "PreviewSnippet.hpp"
#include "RgbColor.hpp"
#include "SyntaxPalette.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <optional>
#include <string_view>

namespace nenenib::ui::win32
{
// 探して見る面のプレビュー欄を描く経路（ADR 0189 の決定 5・採用案
// docs/design/2026-11-16-finder-look.md の「置き方」「プレビュー欄」）。欄の地は
// background、当たりの 欄との境（right は左の縦・below は上の横）と見出しの下の線は
// panel_border、見出しの名前は text の semibold で場所・行・言語は muted、行番号は
// gutter（当たりの行は text）、当たりの行は current_line の面、当たりの文字は search の面 ＋ accent
// の 1 DIP の枠。行は折り返さず欄の右端で切る。見出しの 右端の印は描いている形（right
// は縦の仕切り・below は横の仕切り）。配置は core::palette_layout と palette_preview_* の 1
// 本。色は Palette のトークンだけ（ADR 0008）。 文字組みは欄の専用の保持（PaletteRowLayouts
// の別の実体・上限 layout_limit）から引き、本文の BodyTextLayouts と面の行の保持には入れない（決定
// 5）。target は製品では Direct2D の描画先、 tests/ui は WIC の bitmap の描画先。
class PreviewPanePainter final
{
  public:
    // 保持の上限。窓の行（最大 finder_preview_line_limit）ごとに本文と行番号の 2 つ、見出しの 4 つ
    // （名前・場所・行・言語）か中身を出せないときの 1 行。
    static constexpr std::size_t layout_limit = core::finder_preview_line_limit * 2 + 4;

    PreviewPanePainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                       const PreviewPaneText &text, PaletteRowLayouts &layouts) noexcept;
    // 欄を layout の pane に描く。layout.preview が off なら何もしない。snippet が無ければ地と線と
    // 印だけ（候補が無い・選んだ候補が欄を持たない）。
    // 本文の色は syntax の構文のトークン（窓の行の PreviewColor の区間・段 P2）。
    void paint(const std::optional<application::PreviewSnippet> &snippet,
               const core::PaletteLayout &layout, const core::Palette &palette,
               const core::SyntaxPalette &syntax);

  private:
    void paint_ground(const core::PaletteLayout &layout, const core::Palette &palette);
    void paint_toggle(const core::PaletteLayout &layout, const core::Palette &palette);
    void paint_heading(const application::PreviewSnippet &snippet,
                       const core::PaletteLayout &layout, const core::Palette &palette);
    // 見出しの右から順に文を置き、残った左端の x を返す。
    [[nodiscard]] std::int32_t paint_heading_right(const application::PreviewSnippet &snippet,
                                                   const core::LayoutRect &heading,
                                                   const core::Palette &palette);
    void paint_lines(const application::PreviewSnippet &snippet, const core::PaletteLayout &layout,
                     const core::Palette &palette);
    void paint_line(const application::PreviewLine &line, const core::LayoutRect &area,
                    const core::Palette &palette);
    void mark_match(IDWriteTextLayout &layout, const application::PreviewLine &line,
                    const core::LayoutRect &area, const core::Palette &palette);
    // 構文の色の区間ごとに、区間の矩形で切り抜いて layout をその色で描き直す（本文の
    // BodyTextPainter::tint_runs と同じ道）。
    void tint_colors(IDWriteTextLayout &layout, const application::PreviewLine &line,
                     const core::LayoutRect &area);
    // 文の幅（物理画素・測れなければ 0）。
    [[nodiscard]] static std::int32_t measured_width(IDWriteTextLayout &layout);
    void line_between(D2D1_POINT_2F from, D2D1_POINT_2F to, core::RgbColor color);
    void draw(IDWriteTextLayout &layout, std::int32_t x, std::int32_t y, core::RgbColor color);
    [[nodiscard]] PaletteRowLayouts::TextLayout
    obtained(std::string_view text, IDWriteTextFormat *format, const core::LayoutRect &area);
    [[nodiscard]] PaletteRowLayouts::TextLayout
    made(std::string_view text, IDWriteTextFormat *format, const core::LayoutRect &area) const;

    ID2D1RenderTarget &target_;
    ID2D1SolidColorBrush &brush_;
    PreviewPaneText text_;
    PaletteRowLayouts &layouts_;
    const core::SyntaxPalette *syntax_ = nullptr;
};
} // namespace nenenib::ui::win32
