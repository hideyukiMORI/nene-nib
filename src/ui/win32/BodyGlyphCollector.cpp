#include "BodyGlyphCollector.hpp"
#include <algorithm>
#include <cmath>
#include <dwrite_1.h>
#include <span>
#include <utility>
namespace nenenib::ui::win32
{
BodyGlyphCollector::BodyGlyphCollector(FLOAT width) : width_(width) {}
std::vector<BodyGlyphRun> BodyGlyphCollector::take()
{
    return std::move(runs_);
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

bool BodyGlyphCollector::outside(const DWRITE_GLYPH_RUN &run, FLOAT x) const
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
    const float extent = std::max(std::abs(static_cast<float>(metrics.glyphBoxLeft)),
                                  std::abs(static_cast<float>(metrics.glyphBoxRight)));
    float bound = extent * run.fontEmSize / static_cast<float>(metrics.designUnitsPerEm) + 2;
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
    bound += offset;
    return x + bound < 0 || x - bound > width_;
}
HRESULT BodyGlyphCollector::collect(const DWRITE_GLYPH_RUN &run, D2D1_POINT_2F origin,
                                    DWRITE_MEASURING_MODE mode)
{
    if (outside(run, origin.x))
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
// Waiver: WVR-0002
// NOLINTNEXTLINE(readability-function-size)
HRESULT STDMETHODCALLTYPE BodyGlyphCollector::DrawGlyphRun(void *, FLOAT x, FLOAT y,
                                                           DWRITE_MEASURING_MODE mode,
                                                           const DWRITE_GLYPH_RUN *run,
                                                           const DWRITE_GLYPH_RUN_DESCRIPTION *,
                                                           IUnknown *effect) noexcept
{
    if (effect != nullptr)
    {
        return E_NOTIMPL;
    }
    return collect(*run, D2D1_POINT_2F{x, y}, mode);
}
// Waiver: WVR-0002
// NOLINTNEXTLINE(readability-function-size)
HRESULT STDMETHODCALLTYPE BodyGlyphCollector::DrawUnderline(void *, FLOAT, FLOAT,
                                                            const DWRITE_UNDERLINE *,
                                                            IUnknown *) noexcept
{
    return E_NOTIMPL;
}
// Waiver: WVR-0002
// NOLINTNEXTLINE(readability-function-size)
HRESULT STDMETHODCALLTYPE BodyGlyphCollector::DrawStrikethrough(void *, FLOAT, FLOAT,
                                                                const DWRITE_STRIKETHROUGH *,
                                                                IUnknown *) noexcept
{
    return E_NOTIMPL;
}
// Waiver: WVR-0002
// NOLINTNEXTLINE(readability-function-size)
HRESULT STDMETHODCALLTYPE BodyGlyphCollector::DrawInlineObject(void *, FLOAT, FLOAT,
                                                               IDWriteInlineObject *, BOOL, BOOL,
                                                               IUnknown *) noexcept
{
    return E_NOTIMPL;
}
} // namespace nenenib::ui::win32
