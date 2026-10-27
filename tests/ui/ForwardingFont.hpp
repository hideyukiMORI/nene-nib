#pragma once
#include <dwrite.h>
#include <memory>
#include <wrl/client.h>
#include <wrl/implements.h>

namespace nenenib::tests::ui
{
// 本物の字体へ全部を委ねる替え玉の字体（Issue #889）。OS の字体選択が同じ family でも呼ぶたびに
// 別の IDWriteFont を返す形を試験で作るために、包むたびに別の物になる。family を読んだ回数
// （GetFontFamily）を共有の数に足す。
class ForwardingFont final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteFont>
{
  public:
    ForwardingFont(Microsoft::WRL::ComPtr<IDWriteFont> inner,
                   std::shared_ptr<UINT32> family_reads) noexcept;
    HRESULT STDMETHODCALLTYPE GetFontFamily(IDWriteFontFamily **family) noexcept override;
    DWRITE_FONT_WEIGHT STDMETHODCALLTYPE GetWeight() noexcept override;
    DWRITE_FONT_STRETCH STDMETHODCALLTYPE GetStretch() noexcept override;
    DWRITE_FONT_STYLE STDMETHODCALLTYPE GetStyle() noexcept override;
    BOOL STDMETHODCALLTYPE IsSymbolFont() noexcept override;
    HRESULT STDMETHODCALLTYPE GetFaceNames(IDWriteLocalizedStrings **names) noexcept override;
    HRESULT STDMETHODCALLTYPE GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_ID id,
                                                      IDWriteLocalizedStrings **strings,
                                                      BOOL *exists) noexcept override;
    DWRITE_FONT_SIMULATIONS STDMETHODCALLTYPE GetSimulations() noexcept override;
    void STDMETHODCALLTYPE GetMetrics(DWRITE_FONT_METRICS *metrics) noexcept override;
    HRESULT STDMETHODCALLTYPE HasCharacter(UINT32 value, BOOL *exists) noexcept override;
    HRESULT STDMETHODCALLTYPE CreateFontFace(IDWriteFontFace **face) noexcept override;

  private:
    Microsoft::WRL::ComPtr<IDWriteFont> inner_;
    std::shared_ptr<UINT32> family_reads_;
};
} // namespace nenenib::tests::ui
