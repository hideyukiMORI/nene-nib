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

[[nodiscard]] float largest_advance_offset(const DWRITE_GLYPH_RUN &run)
{
    float offset = 0;
    if (run.glyphOffsets != nullptr)
    {
        for (const auto value : std::span(run.glyphOffsets, run.glyphCount))
        {
            offset = std::max(offset, std::abs(value.advanceOffset));
        }
    }
    return offset;
}

[[nodiscard]] float glyph_box_extent(const DWRITE_GLYPH_RUN &run,
                                     const DWRITE_FONT_METRICS1 &metrics)
{
    const float extent = std::max(std::abs(static_cast<float>(metrics.glyphBoxLeft)),
                                  std::abs(static_cast<float>(metrics.glyphBoxRight)));
    return scaled(extent, run, metrics);
}

// 原点から左右へ字形が届きうる最大の幅（ADR 0073 の保守的な境界）。
[[nodiscard]] float horizontal_reach(const DWRITE_GLYPH_RUN &run,
                                     const DWRITE_FONT_METRICS1 &metrics)
{
    float bound = glyph_box_extent(run, metrics) + 2;
    for (const auto advance : std::span(run.glyphAdvances, run.glyphCount))
    {
        bound += std::abs(advance);
    }
    return bound + largest_advance_offset(run);
}

// 字形 1 つの送りの区間から左右へ字の箱がはみ出しうる最大（全字形の箱の大きい方・
// run の advanceOffset の最大・2）。run の単位の境界から送りの和を除いたもの。
[[nodiscard]] float glyph_reach(const DWRITE_GLYPH_RUN &run, const DWRITE_FONT_METRICS1 &metrics)
{
    return glyph_box_extent(run, metrics) + 2 + largest_advance_offset(run);
}

// 横向きの run と寸法を読めない face は nullopt（除かない・切らない）。
[[nodiscard]] std::optional<DWRITE_FONT_METRICS1> metrics_of(const DWRITE_GLYPH_RUN &run)
{
    Microsoft::WRL::ComPtr<IDWriteFontFace1> face;
    if (run.isSideways || FAILED(run.fontFace->QueryInterface(IID_PPV_ARGS(&face))))
    {
        return std::nullopt;
    }
    DWRITE_FONT_METRICS1 metrics{};
    face->GetMetrics(&metrics);
    if (metrics.designUnitsPerEm == 0)
    {
        return std::nullopt;
    }
    return metrics;
}

// 字形の区間 [first, last)。
using GlyphSpan = std::pair<std::size_t, std::size_t>;

// 字形の送りの区間を reach だけ広げ、画面 [left, right] の左より左で終わる先頭の字形と
// 右より右で始まる末尾の字形を除いた区間。送りは左から積む（DirectWrite と同じ和の順）。
[[nodiscard]] GlyphSpan visible_glyphs(const BodyGlyphRun &run, float reach, float left,
                                       float right)
{
    std::size_t first = 0;
    std::size_t last = 0;
    float pen = run.origin.x;
    for (std::size_t glyph = 0; glyph < run.advances.size(); ++glyph)
    {
        const float next = pen + run.advances[glyph];
        if (first == glyph && std::max(pen, next) + reach < left)
        {
            first = glyph + 1;
        }
        if (std::min(pen, next) - reach <= right)
        {
            last = glyph + 1;
        }
        pen = next;
    }
    return {first, std::max(first, last)};
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

bool BodyGlyphCollector::outside(const DWRITE_GLYPH_RUN &run, const DWRITE_FONT_METRICS1 &metrics,
                                 D2D1_POINT_2F origin) const
{
    const float bound = horizontal_reach(run, metrics);
    if (origin.x + bound < left_ || origin.x - bound > width_)
    {
        return true;
    }
    const auto [above, below] = vertical_reach(run, metrics);
    return origin.y + below < top_ || origin.y - above > bottom_;
}
HRESULT BodyGlyphCollector::collect(const DWRITE_GLYPH_RUN &run,
                                    const DWRITE_GLYPH_RUN_DESCRIPTION *description,
                                    D2D1_POINT_2F origin, DWRITE_MEASURING_MODE mode)
{
    // Tabは原点を進めるだけの0字形run。Direct2Dへ渡すとE_INVALIDARGになる。
    if (run.glyphCount == 0)
    {
        return S_OK;
    }
    const auto metrics = metrics_of(run);
    if (metrics.has_value() && outside(run, metrics.value(), origin))
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
                        {},
                        0,
                        {},
                        0,
                        0,
                        run.glyphCount};
    for (const auto advance : advances)
    {
        stored.advance += advance;
    }
    if (description != nullptr && description->clusterMap != nullptr)
    {
        const auto clusters = std::span(description->clusterMap, description->stringLength);
        stored.text_position = description->textPosition;
        stored.clusters.assign(clusters.begin(), clusters.end());
    }
    if (run.glyphOffsets != nullptr)
    {
        const auto offsets = std::span(run.glyphOffsets, run.glyphCount);
        stored.offsets.assign(offsets.begin(), offsets.end());
    }
    if (metrics.has_value())
    {
        trim(stored, glyph_reach(run, metrics.value()));
    }
    if (stored.first < stored.last)
    {
        runs_.push_back(std::move(stored));
    }
    return S_OK;
}
// 右から左の run は字形の順と文字の順が逆なので切らない（保守的）。1 字形の run も切らない。
// 字形の配列は全体のまま持ち、描く窓だけを狭める。縦は run の単位のまま（#528）。
void BodyGlyphCollector::trim(BodyGlyphRun &stored, float reach) const
{
    if (stored.bidi % 2 != 0 || stored.indices.size() < 2)
    {
        return;
    }
    const auto [first, last] = visible_glyphs(stored, reach, left_, width_);
    stored.first = first;
    stored.last = last;
}
// SDK-ABI: IDWriteTextRenderer::DrawGlyphRun
// NOLINTNEXTLINE(readability-function-size)
HRESULT STDMETHODCALLTYPE BodyGlyphCollector::DrawGlyphRun(
    void *, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE mode, const DWRITE_GLYPH_RUN *run,
    const DWRITE_GLYPH_RUN_DESCRIPTION *description, IUnknown *effect) noexcept
{
    if (effect != nullptr)
    {
        return settle(E_NOTIMPL);
    }
    return settle(collect(*run, description, D2D1_POINT_2F{x, y}, mode));
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
