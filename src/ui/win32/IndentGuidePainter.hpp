#pragma once

#include "IndentGuide.hpp"
#include "IndentGuidePaint.hpp"
#include "IndentGuideRow.hpp"
#include "RgbColor.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <d2d1.h>
#include <utility>

namespace nenenib::ui::win32
{
// 本文の「下の飾り」のインデントガイド（ADR 0193 の決定 2・5・9・#1975）。保持した layout の外の
// 矩形だけで描き、BodyTextLayouts の鍵と字形に触れない。段 i の線は x = left + i × step × space
// （行頭の空白と Tab だけなので hit test を回さない・Tab の止まりは空白の幅の倍数）の左端から太さ
// thin（強める段は thick）の縦長の矩形で、最初の画面行の高さだけ。回すのは本文の欄の左右の端の間の
// 段だけで、端の外の段は割り算で飛ばす（字下げの深い行でも矩形の数は見えている幅で決まる・
// #1975 のレビュー R1）。色は core の indent_guide_color の 1 本から引く（ui
// に色のリテラルを書かない・ ADR 0008 の決定 8）。Target は ID2D1RenderTarget
// の同じ名前のメソッドを持つ描画先（試験はメモリの bitmap）。
class IndentGuidePainter final
{
  public:
    template <typename Target>
    static void paint(Target &target, ID2D1SolidColorBrush *brush, const IndentGuideRow &row,
                      const core::IndentGuidePaint &paint)
    {
        const auto &guides = row.guides;
        const auto [first, end] = shown_levels(row);
        for (std::size_t level = first; level < end; ++level)
        {
            const bool strong = guides.active.has_value() && guides.active.value() == level;
            const float x = x_of(row, level);
            const float width = strong ? row.thick : row.thin;
            brush->SetColor(color_of(core::indent_guide_color(paint, level, strong)));
            target.FillRectangle(D2D1::RectF(x, row.top, x + width, row.top + row.height), brush);
        }
    }

    // 描く段の区間 [first, end)。左端より左（線の右端が欄の左端に届かない段）と右端から先の段を、
    // 段の間隔 step × space の割り算で外す（段を回して数えない）。
    [[nodiscard]] static std::pair<std::size_t, std::size_t>
    shown_levels(const IndentGuideRow &row) noexcept
    {
        const auto levels = static_cast<float>(row.guides.levels);
        const float pitch = static_cast<float>(row.guides.step) * row.space;
        if (row.guides.levels == 0 || pitch <= 0.0F)
        {
            return {0, 0};
        }
        // 線は floor した x から太さ thick までを塗るので、x + thick が左端を越える段から。
        const float before = std::ceil((row.clip_left - row.thick - row.left) / pitch);
        // floor(left + i × pitch) < clip_right（clip_right は整数の画素）の段まで。
        const float after = std::ceil((row.clip_right - row.left) / pitch);
        const float first = std::clamp(before, 0.0F, levels);
        const float end = std::clamp(after, first, levels);
        return {static_cast<std::size_t>(first), static_cast<std::size_t>(end)};
    }

  private:
    [[nodiscard]] static float x_of(const IndentGuideRow &row, std::size_t level) noexcept
    {
        return std::floor(row.left + (static_cast<float>(level * row.guides.step) * row.space));
    }

    [[nodiscard]] static D2D1_COLOR_F color_of(core::RgbColor value) noexcept
    {
        constexpr float channel = 255.0F;
        return D2D1::ColorF(static_cast<float>(value.red) / channel,
                            static_cast<float>(value.green) / channel,
                            static_cast<float>(value.blue) / channel);
    }
};
} // namespace nenenib::ui::win32
