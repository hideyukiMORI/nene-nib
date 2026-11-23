#pragma once

#include "IndentGuide.hpp"
#include "IndentGuidePaint.hpp"
#include "IndentGuideRow.hpp"
#include "RgbColor.hpp"

#include <cmath>
#include <cstddef>
#include <d2d1.h>

namespace nenenib::ui::win32
{
// 本文の「下の飾り」のインデントガイド（ADR 0193 の決定 2・5・9・#1975）。保持した layout の外の
// 矩形だけで描き、BodyTextLayouts の鍵と字形に触れない。段 i の線は x = left + i × step × space
// （行頭の空白と Tab だけなので hit test を回さない・Tab の止まりは空白の幅の倍数）の左端から太さ
// thin（強める段は thick）の縦長の矩形で、最初の画面行の高さだけ。色は core の
// indent_guide_color の 1 本から引く（ui に色のリテラルを書かない・ADR 0008 の決定 8）。Target は
// ID2D1RenderTarget の同じ名前のメソッドを持つ描画先（試験はメモリの bitmap）。
class IndentGuidePainter final
{
  public:
    template <typename Target>
    static void paint(Target &target, ID2D1SolidColorBrush *brush, const IndentGuideRow &row,
                      const core::IndentGuidePaint &paint)
    {
        const auto &guides = row.guides;
        for (std::size_t level = 0; level < guides.levels; ++level)
        {
            const bool strong = guides.active.has_value() && guides.active.value() == level;
            const float x =
                std::floor(row.left + (static_cast<float>(level * guides.step) * row.space));
            const float width = strong ? row.thick : row.thin;
            brush->SetColor(color_of(core::indent_guide_color(paint, level, strong)));
            target.FillRectangle(D2D1::RectF(x, row.top, x + width, row.top + row.height), brush);
        }
    }

  private:
    [[nodiscard]] static D2D1_COLOR_F color_of(core::RgbColor value) noexcept
    {
        constexpr float channel = 255.0F;
        return D2D1::ColorF(static_cast<float>(value.red) / channel,
                            static_cast<float>(value.green) / channel,
                            static_cast<float>(value.blue) / channel);
    }
};
} // namespace nenenib::ui::win32
