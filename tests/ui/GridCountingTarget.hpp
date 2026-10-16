#pragma once
#include <cstddef>
#include <d2d1.h>
#include <dwrite.h>
#include <span>
#include <vector>

namespace nenenib::tests::ui
{
// GridRowPainter の替え玉の描画先（#546・#531 の CountingTarget の形）。描かずに、塗った矩形と
// 字形の左端（run の原点と送りから）と layout の描画を数える。名前と引数は ID2D1RenderTarget の
// 同じ名前のメソッドに合わせる。
class GridCountingTarget final
{
  public:
    void FillRectangle(const D2D1_RECT_F &rect, ID2D1Brush * /*brush*/) noexcept
    {
        fills_.push_back(rect);
    }
    void FillRoundedRectangle(const D2D1_ROUNDED_RECT &rect, ID2D1Brush * /*brush*/) noexcept
    {
        fills_.push_back(rect.rect);
    }
    void DrawRectangle(const D2D1_RECT_F &rect, ID2D1Brush * /*brush*/, FLOAT /*stroke*/) noexcept
    {
        outlines_.push_back(rect);
    }
    void DrawGlyphRun(D2D1_POINT_2F origin, const DWRITE_GLYPH_RUN *run, ID2D1Brush * /*brush*/,
                      DWRITE_MEASURING_MODE /*measuring*/)
    {
        ++glyph_runs_;
        float x = origin.x;
        const auto advances = std::span(run->glyphAdvances, run->glyphCount);
        const auto offsets = run->glyphOffsets == nullptr
                                 ? std::span<const DWRITE_GLYPH_OFFSET>{}
                                 : std::span(run->glyphOffsets, run->glyphCount);
        for (std::size_t index = 0; index < advances.size(); ++index)
        {
            glyph_lefts_.push_back(x + (offsets.empty() ? 0.0F : offsets[index].advanceOffset));
            x += advances[index];
        }
    }
    void DrawTextLayout(D2D1_POINT_2F /*origin*/, IDWriteTextLayout * /*layout*/,
                        ID2D1Brush * /*brush*/, D2D1_DRAW_TEXT_OPTIONS /*options*/) noexcept
    {
        ++text_layouts_;
    }
    void PushAxisAlignedClip(const D2D1_RECT_F & /*clip*/, D2D1_ANTIALIAS_MODE /*mode*/) noexcept
    {
        ++depth_;
    }
    void PopAxisAlignedClip() noexcept
    {
        --depth_;
    }
    [[nodiscard]] const std::vector<D2D1_RECT_F> &fills() const noexcept
    {
        return fills_;
    }
    [[nodiscard]] const std::vector<D2D1_RECT_F> &outlines() const noexcept
    {
        return outlines_;
    }
    [[nodiscard]] const std::vector<float> &glyph_lefts() const noexcept
    {
        return glyph_lefts_;
    }
    [[nodiscard]] std::size_t glyph_runs() const noexcept
    {
        return glyph_runs_;
    }
    [[nodiscard]] std::size_t text_layouts() const noexcept
    {
        return text_layouts_;
    }
    // 出した切り抜きをすべて戻したか。
    [[nodiscard]] bool balanced() const noexcept
    {
        return depth_ == 0;
    }

  private:
    std::vector<D2D1_RECT_F> fills_;
    std::vector<D2D1_RECT_F> outlines_;
    std::vector<float> glyph_lefts_;
    std::size_t glyph_runs_ = 0;
    std::size_t text_layouts_ = 0;
    std::ptrdiff_t depth_ = 0;
};
} // namespace nenenib::tests::ui
