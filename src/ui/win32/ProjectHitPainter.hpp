#pragma once

#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "ProjectHitRow.hpp"
#include "ProjectHitText.hpp"
#include "RgbColor.hpp"

#include <cstddef>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <string_view>

namespace nenenib::ui::win32
{
// 全文検索の当たりの行を 2 段で描く経路（ADR 0167 の決定 5・採用案
// docs/design/2026-11-16-finder-look.md の「当たりの行」）。上の段は行の本文（text の色・当たりの
// 文字の後ろに search の面・行頭を省いた窓は先頭に「…」）、下の段は
// `相対パス:行`（muted）。どちらも
// はみ出しは書式の文字単位の「…」で末尾を切る。構文の色は付けない（プレビュー欄と合わせて後で）。
// 選んだ行の面は呼び出し元が描く（今の面と同じ）。配置は core::palette_hit_line と
// palette_hit_place の 1 本。行の文字組みは Ctrl+P の面の保持（PaletteRowLayouts・ADR 0150 の形）
// から引き、新しい保持を作らない。target は製品では Direct2D の描画先、tests/ui は WIC の bitmap の
// 描画先。色は Palette のトークンだけ（ADR 0008）。
class ProjectHitPainter final
{
  public:
    // 1 行が保持に置く文字組みの数（上の段と下の段）。
    static constexpr std::size_t layouts_per_row = 2;

    ProjectHitPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                      const ProjectHitText &text, PaletteRowLayouts &layouts) noexcept;
    // 当たり 1 つを行 area（面の行の矩形）に描く。
    void paint(const core::ProjectHitRow &hit, const core::LayoutRect &area,
               const core::Palette &palette);
    // 上の段の文（省いた窓は先頭に「…」）と、その中の当たりの始まり（バイト）。
    [[nodiscard]] static std::string shown_line(const core::ProjectHitRow &hit);
    [[nodiscard]] static std::size_t shown_offset(const core::ProjectHitRow &hit) noexcept;
    // 下の段の文（`相対パス:行`）。
    [[nodiscard]] static std::string shown_place(const core::ProjectHitRow &hit);

  private:
    void paint_line(const core::ProjectHitRow &hit, const core::LayoutRect &area,
                    const core::Palette &palette);
    // 当たりの文字の後ろに面を敷く（欄で切る）。
    void mark_match(IDWriteTextLayout &layout, const std::string &shown,
                    const core::ProjectHitRow &hit, const core::LayoutRect &area);
    void draw(IDWriteTextLayout &layout, const core::LayoutRect &area, core::RgbColor color);
    [[nodiscard]] PaletteRowLayouts::TextLayout
    obtained(std::string_view text, IDWriteTextFormat *format, const core::LayoutRect &area);
    [[nodiscard]] PaletteRowLayouts::TextLayout
    made(std::string_view text, IDWriteTextFormat *format, const core::LayoutRect &area) const;

    ID2D1RenderTarget &target_;
    ID2D1SolidColorBrush &brush_;
    ProjectHitText text_;
    PaletteRowLayouts &layouts_;
};
} // namespace nenenib::ui::win32
