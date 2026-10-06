#pragma once
#include <dwrite_2.h>
#include <wrl/implements.h>

namespace nenenib::tests::ui
{
// OS の字体選択の役の替え玉。呼ばれた回数を数え、試験が決めた HRESULT を返す。
// scale には何回目の呼び出しかを入れるので、保持から返った答えかどうかを値で見分けられる
// （ADR 0077 の決定 3）。字体は返さない（nullptr）。
class ScriptedFontFallback final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteFontFallback>
{
  public:
    void set_result(HRESULT result) noexcept;
    [[nodiscard]] HRESULT result() const noexcept;
    [[nodiscard]] UINT32 calls() const noexcept;
    HRESULT STDMETHODCALLTYPE MapCharacters(IDWriteTextAnalysisSource *source, UINT32 position,
                                            UINT32 length, IDWriteFontCollection *collection,
                                            const WCHAR *family, DWRITE_FONT_WEIGHT weight,
                                            DWRITE_FONT_STYLE style, DWRITE_FONT_STRETCH stretch,
                                            UINT32 *mapped_length, IDWriteFont **mapped_font,
                                            FLOAT *scale) noexcept override;

  private:
    HRESULT answer(UINT32 length, UINT32 *mapped_length, IDWriteFont **mapped_font, FLOAT *scale);
    HRESULT result_ = S_OK;
    UINT32 calls_ = 0;
};
} // namespace nenenib::tests::ui
