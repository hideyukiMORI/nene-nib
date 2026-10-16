#include "BodyGlyphCollector.hpp"
#include <algorithm>
#include <cmath>
#include <dwrite_1.h>
#include <span>
#include <utility>
namespace nenenib::ui::win32
{
BodyGlyphCollector::BodyGlyphCollector(FLOAT width, FLOAT left, FLOAT top, FLOAT bottom)
    : width_(left + width), left_(left), top_(top), bottom_(bottom)
{
}
std::optional<std::vector<BodyGlyphRun>> BodyGlyphCollector::take()
{
    if (rejected_)
    {
        return std::nullopt;
    }
    return std::move(runs_);
}
HRESULT BodyGlyphCollector::settle(HRESULT result) noexcept
{
    if (FAILED(result))
    {
        rejected_ = true;
    }
    return result;
}
HRESULT STDMETHODCALLTYPE BodyGlyphCollector::IsPixelSnappingDisabled(void *,
                                                                      BOOL *disabled) noexcept
{
    *disabled = FALSE;
    return S_OK;
}
HRESULT STDMETHODCALLTYPE BodyGlyphCollector::GetCurrentTransform(void *,
                                                                  DWRITE_MATRIX *transform) noexcept
{
    *transform = DWRITE_MATRIX{1, 0, 0, 1, 0, 0};
    return S_OK;
}
HRESULT STDMETHODCALLTYPE BodyGlyphCollector::GetPixelsPerDip(void *, FLOAT *pixels) noexcept
{
    *pixels = 1;
    return S_OK;
}

namespace
{
[[nodiscard]] float scaled(float units, const DWRITE_GLYPH_RUN &run,
                           const DWRITE_FONT_METRICS1 &metrics)
{
    return units * run.fontEmSize / static_cast<float>(metrics.designUnitsPerEm);
}

// 原点から左右へ字形が届きうる最大の幅（ADR 0073 の保守的な境界）。
[[nodiscard]] float horizontal_reach(const DWRITE_GLYPH_RUN &run,
                                     const DWRITE_FONT_METRICS1 &metrics)
{
    const float extent = std::max(std::abs(static_cast<float>(metrics.glyphBoxLeft)),
                                  std::abs(static_cast<float>(metrics.glyphBoxRight)));
    float bound = scaled(extent, run, metrics) + 2;
    for (const auto advance : std::span(run.glyphAdvances, run.glyphCount))
    {
        bound += std::abs(advance);
    }
    float offset = 0;
    if (run.glyphOffsets != nullptr)
    {
        for (const auto value : std::span(run.glyphOffsets, run.glyphCount))
        {
            offset = std::max(offset, std::abs(value.advanceOffset));
        }
    }
    return bound + offset;
}

// baseline から上（first）と下（second）へ字形が届きうる長さ。字の箱（ascent / descent と
// 全字形の箱の大きい方）に、余裕として字体の 1 行分（ascent + descent + lineGap）と
// 字形の縦のずれの最大と 2 を足す。除きすぎて消えるより、残して描く（#528）。
[[nodiscard]] std::pair<float, float> vertical_reach(const DWRITE_GLYPH_RUN &run,
                                                     const DWRITE_FONT_METRICS1 &metrics)
{
    const auto ascent = static_cast<float>(metrics.ascent);
    const auto descent = static_cast<float>(metrics.descent);
    const float margin = ascent + descent + std::abs(static_cast<float>(metrics.lineGap));
    const float above = std::max(ascent, static_cast<float>(metrics.glyphBoxTop)) + margin;
    const float below = std::max(descent, -static_cast<float>(metrics.glyphBoxBottom)) + margin;
    float offset = 0;
    if (run.glyphOffsets != nullptr)
    {
        for (const auto value : std::span(run.glyphOffsets, run.glyphCount))
        {
            offset = std::max(offset, std::abs(value.ascenderOffset));
        }
    }
    return {scaled(above, run, metrics) + offset + 2, scaled(below, run, metrics) + offset + 2};
}
} // namespace

bool BodyGlyphCollector::outside(const DWRITE_GLYPH_RUN &run, D2D1_POINT_2F origin) const
{
    Microsoft::WRL::ComPtr<IDWriteFontFace1> face;
    if (run.isSideways || FAILED(run.fontFace->QueryInterface(IID_PPV_ARGS(&face))))
    {
        return false;
    }
    DWRITE_FONT_METRICS1 metrics{};
    face->GetMetrics(&metrics);
    if (metrics.designUnitsPerEm == 0)
    {
        return false;
    }
    const float bound = horizontal_reach(run, metrics);
    if (origin.x + bound < left_ || origin.x - bound > width_)
    {
        return true;
    }
    const auto [above, below] = vertical_reach(run, metrics);
    return origin.y + below < top_ || origin.y - above > bottom_;
}
HRESULT BodyGlyphCollector::collect(const DWRITE_GLYPH_RUN &run, D2D1_POINT_2F origin,
                                    DWRITE_MEASURING_MODE mode)
{
    // Tabは原点を進めるだけの0字形run。Direct2Dへ渡すとE_INVALIDARGになる。
    if (run.glyphCount == 0 || outside(run, origin))
    {
        return S_OK;
    }
    const auto indices = std::span(run.glyphIndices, run.glyphCount);
    const auto advances = std::span(run.glyphAdvances, run.glyphCount);
    BodyGlyphRun stored{run.fontFace,
                        run.fontEmSize,
                        run.isSideways,
                        run.bidiLevel,
                        origin,
                        mode,
                        {indices.begin(), indices.end()},
                        {advances.begin(), advances.end()},
                        {}};
    if (run.glyphOffsets != nullptr)
    {
        const auto offsets = std::span(run.glyphOffsets, run.glyphCount);
        stored.offsets.assign(offsets.begin(), offsets.end());
    }
    runs_.push_back(std::move(stored));
    return S_OK;
}
// SDK-ABI: IDWriteTextRenderer::DrawGlyphRun
// NOLINTNEXTLINE(readability-function-size)
HRESULT STDMETHODCALLTYPE BodyGlyphCollector::DrawGlyphRun(void *, FLOAT x, FLOAT y,
                                                           DWRITE_MEASURING_MODE mode,
                                                           const DWRITE_GLYPH_RUN *run,
                                                           const DWRITE_GLYPH_RUN_DESCRIPTION *,
                                                           IUnknown *effect) noexcept
{
    if (effect != nullptr)
    {
        return settle(E_NOTIMPL);
    }
    return settle(collect(*run, D2D1_POINT_2F{x, y}, mode));
}
// SDK-ABI: IDWriteTextRenderer::DrawUnderline
// NOLINTNEXTLINE(readability-function-size)
HRESULT STDMETHODCALLTYPE BodyGlyphCollector::DrawUnderline(void *, FLOAT, FLOAT,
                                                            const DWRITE_UNDERLINE *,
                                                            IUnknown *) noexcept
{
    return settle(E_NOTIMPL);
}
// SDK-ABI: IDWriteTextRenderer::DrawStrikethrough
// NOLINTNEXTLINE(readability-function-size)
HRESULT STDMETHODCALLTYPE BodyGlyphCollector::DrawStrikethrough(void *, FLOAT, FLOAT,
                                                                const DWRITE_STRIKETHROUGH *,
                                                                IUnknown *) noexcept
{
    return settle(E_NOTIMPL);
}
// SDK-ABI: IDWriteTextRenderer::DrawInlineObject
// NOLINTNEXTLINE(readability-function-size)
HRESULT STDMETHODCALLTYPE BodyGlyphCollector::DrawInlineObject(void *, FLOAT, FLOAT,
                                                               IDWriteInlineObject *, BOOL, BOOL,
                                                               IUnknown *) noexcept
{
    return settle(E_NOTIMPL);
}
} // namespace nenenib::ui::win32
