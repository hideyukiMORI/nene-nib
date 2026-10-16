#pragma once
#include "BodyGlyphRun.hpp"
#include <dwrite_1.h>
#include <limits>
#include <optional>
#include <wrl/implements.h>
namespace nenenib::ui::win32
{
class BodyGlyphCollector final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextRenderer>
{
  public:
    // top / bottom は描く範囲の縦（layout の座標）。既定は上下に限りなし（位置解決など、
    // 描く範囲を渡さない呼び手は今までどおり縦に除かない）。
    explicit BodyGlyphCollector(FLOAT width, FLOAT left = 0,
                                FLOAT top = std::numeric_limits<FLOAT>::lowest(),
                                FLOAT bottom = std::numeric_limits<FLOAT>::max());
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
                                           const DWRITE_GLYPH_RUN_DESCRIPTION *description,
                                           IUnknown *effect) noexcept override;
    HRESULT STDMETHODCALLTYPE DrawUnderline(void *, FLOAT, FLOAT, const DWRITE_UNDERLINE *,
                                            IUnknown *) noexcept override;
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void *, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH *,
                                                IUnknown *) noexcept override;
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void *, FLOAT, FLOAT, IDWriteInlineObject *, BOOL,
                                               BOOL, IUnknown *) noexcept override;

  private:
    [[nodiscard]] bool outside(const DWRITE_GLYPH_RUN &run, const DWRITE_FONT_METRICS1 &metrics,
                               D2D1_POINT_2F origin) const;
    // 横に画面と交わる run の描く窓を、画面外の先頭と末尾の字形を除いた区間にする
    // （ADR 0073 の 2026-10-16 の補足）。
    void trim(BodyGlyphRun &stored, float reach) const;
    // 直前に保持した run に stored を後ろからつなげたら true（ADR 0073 の 2026-10-16 の決定）。
    [[nodiscard]] bool joined(const BodyGlyphRun &stored, std::optional<float> reach);
    // 末尾を閉じて、stored を新しい末尾として保持する（窓は閉じるときに決める）。
    void open_tail(BodyGlyphRun stored, std::optional<float> reach);
    void close_tail();
    HRESULT collect(const DWRITE_GLYPH_RUN &run, const DWRITE_GLYPH_RUN_DESCRIPTION *description,
                    D2D1_POINT_2F origin, DWRITE_MEASURING_MODE mode);
    // 失敗の HRESULT なら棄却の印を立てる。HRESULT はそのまま返す。
    HRESULT settle(HRESULT result) noexcept;
    FLOAT width_;
    FLOAT left_;
    FLOAT top_;
    FLOAT bottom_;
    std::vector<BodyGlyphRun> runs_;
    // runs_ の末尾が直前の callback の run で、まだ描く窓を決めていないときだけ true。末尾の
    // 送りを左から積んだ右端と、窓を決める保守の幅（寸法を読めない face は nullopt）を持つ。
    bool tail_open_ = false;
    FLOAT tail_pen_ = 0;
    std::optional<float> tail_reach_;
    bool rejected_ = false;
};
} // namespace nenenib::ui::win32
