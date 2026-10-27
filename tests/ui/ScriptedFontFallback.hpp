#pragma once
#include <dwrite_2.h>
#include <memory>
#include <wrl/client.h>
#include <wrl/implements.h>

namespace nenenib::tests::ui
{
// OS の字体選択の役の替え玉。呼ばれた回数を数え、試験が決めた HRESULT を返す。
// scale には何回目の呼び出しかを入れるので、保持から返った答えかどうかを値で見分けられる
// （ADR 0077 の決定 3）。字体は既定で返さない（nullptr）。
// 答えの字体を置くと、その字体を scale 1 で返す（OS
// の字体選択に依らない検査のため・#889）。委ね先を置くと、答えは委ね先のものになり、
// 渡された長さの最大と呼ばれた回数だけを数える（ADR 0142 の決定 3 と追補）。包むと、委ね先が返した
// 字体を呼ぶたびに別の物（ForwardingFont）に包み、family を読んだ回数を数える（#889）。
class ScriptedFontFallback final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteFontFallback>
{
  public:
    void set_result(HRESULT result) noexcept;
    [[nodiscard]] HRESULT result() const noexcept;
    [[nodiscard]] UINT32 calls() const noexcept;
    void delegate_to(Microsoft::WRL::ComPtr<IDWriteFontFallback> delegate) noexcept;
    void forget_longest() noexcept;
    [[nodiscard]] UINT32 longest() const noexcept;
    void answer_with(Microsoft::WRL::ComPtr<IDWriteFont> font) noexcept;
    void wrap_fonts() noexcept;
    [[nodiscard]] UINT32 family_reads() const noexcept;
    HRESULT STDMETHODCALLTYPE MapCharacters(IDWriteTextAnalysisSource *source, UINT32 position,
                                            UINT32 length, IDWriteFontCollection *collection,
                                            const WCHAR *family, DWRITE_FONT_WEIGHT weight,
                                            DWRITE_FONT_STYLE style, DWRITE_FONT_STRETCH stretch,
                                            UINT32 *mapped_length, IDWriteFont **mapped_font,
                                            FLOAT *scale) noexcept override;

  private:
    HRESULT answer(UINT32 length, UINT32 *mapped_length, IDWriteFont **mapped_font, FLOAT *scale);
    void counted(UINT32 length) noexcept;
    HRESULT wrapped(HRESULT result, IDWriteFont **mapped_font) noexcept;
    HRESULT result_ = S_OK;
    UINT32 calls_ = 0;
    UINT32 longest_ = 0;
    Microsoft::WRL::ComPtr<IDWriteFontFallback> delegate_;
    Microsoft::WRL::ComPtr<IDWriteFont> answer_font_;
    bool wraps_ = false;
    std::shared_ptr<UINT32> family_reads_ = std::make_shared<UINT32>(0U);
};
} // namespace nenenib::tests::ui
