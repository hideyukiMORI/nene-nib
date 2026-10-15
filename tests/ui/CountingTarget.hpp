#pragma once
#include <cstddef>
#include <d2d1.h>
#include <dwrite.h>

namespace nenenib::tests::ui
{
// BodyTextPainter の替え玉の描画先（#531）。描かずに、字形 run と layout の描画の呼び出しと
// 切り抜きの出し入れを数える。名前と引数は ID2D1RenderTarget の同じ名前のメソッドに合わせる。
class CountingTarget final
{
  public:
    void DrawGlyphRun(D2D1_POINT_2F /*origin*/, const DWRITE_GLYPH_RUN * /*run*/,
                      ID2D1Brush * /*brush*/, DWRITE_MEASURING_MODE /*measuring*/) noexcept
    {
        ++glyph_runs_;
    }
    void DrawTextLayout(D2D1_POINT_2F /*origin*/, IDWriteTextLayout * /*layout*/,
                        ID2D1Brush * /*brush*/, D2D1_DRAW_TEXT_OPTIONS /*options*/) noexcept
    {
        ++text_layouts_;
    }
    void PushAxisAlignedClip(const D2D1_RECT_F & /*clip*/, D2D1_ANTIALIAS_MODE /*mode*/) noexcept
    {
        ++clips_;
        ++depth_;
    }
    void PopAxisAlignedClip() noexcept
    {
        --depth_;
    }
    [[nodiscard]] std::size_t glyph_runs() const noexcept
    {
        return glyph_runs_;
    }
    [[nodiscard]] std::size_t text_layouts() const noexcept
    {
        return text_layouts_;
    }
    [[nodiscard]] std::size_t clips() const noexcept
    {
        return clips_;
    }
    // 出した切り抜きをすべて戻したか。
    [[nodiscard]] bool balanced() const noexcept
    {
        return depth_ == 0;
    }

  private:
    std::size_t glyph_runs_ = 0;
    std::size_t text_layouts_ = 0;
    std::size_t clips_ = 0;
    std::ptrdiff_t depth_ = 0;
};
} // namespace nenenib::tests::ui
