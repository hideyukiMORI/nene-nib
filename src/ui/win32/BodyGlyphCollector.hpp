#pragma once
#include "BodyGlyphRun.hpp"
#include <optional>
#include <wrl/implements.h>
namespace nenenib::ui::win32
{
class BodyGlyphCollector final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextRenderer>
{
  public:
    explicit BodyGlyphCollector(FLOAT width);
    // 集めた字形。未対応の callback を 1 つでも受けたら（棄却の印）、部分の字形を渡さず nullopt。
    // IDWriteTextLayout::Draw は callback の HRESULT を無視して S_OK を返すので、
    // 字形で描けるかはここだけが決める（ADR 0073 の追記）。
    [[nodiscard]] std::optional<std::vector<BodyGlyphRun>> take();
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
    // 失敗の HRESULT なら棄却の印を立てる。HRESULT はそのまま返す。
    HRESULT settle(HRESULT result) noexcept;
    FLOAT width_;
    std::vector<BodyGlyphRun> runs_;
    bool rejected_ = false;
};
} // namespace nenenib::ui::win32
