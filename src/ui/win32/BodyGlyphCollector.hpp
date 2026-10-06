#pragma once
#include "BodyGlyphRun.hpp"
#include <wrl/implements.h>
namespace nenenib::ui::win32
{
class BodyGlyphCollector final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextRenderer>
{
  public:
    explicit BodyGlyphCollector(FLOAT width);
    [[nodiscard]] std::vector<BodyGlyphRun> take();
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void *, BOOL *disabled) noexcept override;
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void *,
                                                  DWRITE_MATRIX *transform) noexcept override;
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void *, FLOAT *pixels) noexcept override;
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void *, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE mode,
                                           const DWRITE_GLYPH_RUN *run,
                                           const DWRITE_GLYPH_RUN_DESCRIPTION *,
                                           IUnknown *effect) noexcept override;
    HRESULT STDMETHODCALLTYPE DrawUnderline(void *, FLOAT, FLOAT, const DWRITE_UNDERLINE *,
                                            IUnknown *) noexcept override;
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void *, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH *,
                                                IUnknown *) noexcept override;
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void *, FLOAT, FLOAT, IDWriteInlineObject *, BOOL,
                                               BOOL, IUnknown *) noexcept override;

  private:
    [[nodiscard]] bool outside(const DWRITE_GLYPH_RUN &run, FLOAT x) const;
    HRESULT collect(const DWRITE_GLYPH_RUN &run, D2D1_POINT_2F origin, DWRITE_MEASURING_MODE mode);
    FLOAT width_;
    std::vector<BodyGlyphRun> runs_;
};
} // namespace nenenib::ui::win32
