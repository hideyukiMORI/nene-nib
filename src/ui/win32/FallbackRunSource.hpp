#pragma once
#include <dwrite.h>
#include <string>
#include <string_view>
#include <wrl/implements.h>
namespace nenenib::ui::win32
{
// 主字体に無い文字の 1 つの連続区間を、OS の字体選択（MapCharacters）へ渡す本文（ADR 0142）。
// 区間の文字列だけを持ち前後の文脈は無いので、FontFallbackCache の保持の鍵になる（ADR 0071）。
// 言語と向きは本文の書式と同じ。数字の置換は無い（本文の書式も持たない）。
class FallbackRunSource final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextAnalysisSource>
{
  public:
    FallbackRunSource(std::wstring language, DWRITE_READING_DIRECTION direction);
    void reset(std::wstring_view text);
    [[nodiscard]] UINT32 length() const noexcept;

    HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 position, const WCHAR **text,
                                                UINT32 *length) noexcept override;
    HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 position, const WCHAR **text,
                                                    UINT32 *length) noexcept override;
    DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() noexcept override;
    HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 position, UINT32 *length,
                                            const WCHAR **name) noexcept override;
    HRESULT STDMETHODCALLTYPE
    GetNumberSubstitution(UINT32 position, UINT32 *length,
                          IDWriteNumberSubstitution **substitution) noexcept override;

  private:
    [[nodiscard]] UINT32 after(UINT32 position) const noexcept;
    std::wstring text_;
    std::wstring language_;
    DWRITE_READING_DIRECTION direction_;
};
} // namespace nenenib::ui::win32
