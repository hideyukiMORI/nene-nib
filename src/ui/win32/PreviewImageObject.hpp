#pragma once

#include <dwrite.h>
#include <wrl/implements.h>

namespace nenenib::ui::win32
{
// プレビューの layout の中で画像の場所を取る inline の物（施主決定 D79・ADR 0155 の追補 62・
// #1439）。大きさ（物理画素）だけを DirectWrite に答え、baseline は高さと同じ（箱の下端を行の
// baseline に置く）。自分では何も描かない（画素は PreviewPainter が layout を描いた後に
// preview_inline_origin の位置へ DrawBitmap で描く）。前後で行を分けてよい（語の境と同じ）。
class PreviewImageObject final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteInlineObject>
{
  public:
    PreviewImageObject(FLOAT width, FLOAT height) noexcept;
    HRESULT STDMETHODCALLTYPE Draw(void *, IDWriteTextRenderer *, FLOAT, FLOAT, BOOL, BOOL,
                                   IUnknown *) noexcept override;
    HRESULT STDMETHODCALLTYPE GetMetrics(DWRITE_INLINE_OBJECT_METRICS *metrics) noexcept override;
    HRESULT STDMETHODCALLTYPE
    GetOverhangMetrics(DWRITE_OVERHANG_METRICS *overhangs) noexcept override;
    HRESULT STDMETHODCALLTYPE GetBreakConditions(DWRITE_BREAK_CONDITION *before,
                                                 DWRITE_BREAK_CONDITION *after) noexcept override;

  private:
    FLOAT width_;
    FLOAT height_;
};
} // namespace nenenib::ui::win32
