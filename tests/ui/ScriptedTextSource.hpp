#pragma once
#include "SourceFault.hpp"

#include <cstdint>
#include <dwrite.h>
#include <string>
#include <wrl/implements.h>

namespace nenenib::tests::ui
{
// DirectWrite が字体選択に渡す本文の替え玉。本文・言語・数字の置換・向きと、前提の崩し方を
// 試験が決める（ADR 0077 の決定 3）。
class ScriptedTextSource final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextAnalysisSource>
{
  public:
    void set_text(std::wstring text);
    void set_language(std::wstring language);
    void set_substitution(Microsoft::WRL::ComPtr<IDWriteNumberSubstitution> substitution);
    void set_direction(DWRITE_READING_DIRECTION direction) noexcept;
    // GetLocaleName と GetNumberSubstitution が答える同じ値の長さの上限。
    void set_property_lengths(UINT32 language, UINT32 substitution) noexcept;
    void set_fault(SourceFault fault) noexcept;
    [[nodiscard]] UINT32 length() const noexcept;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interface_id, void **object) noexcept override;
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
    [[nodiscard]] UINT32 bounded(UINT32 limit) const noexcept;
    std::wstring text_ = L"日本語";
    std::wstring language_ = L"ja-jp";
    Microsoft::WRL::ComPtr<IDWriteNumberSubstitution> substitution_;
    DWRITE_READING_DIRECTION direction_ = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    UINT32 language_limit_ = UINT32_MAX;
    UINT32 substitution_limit_ = UINT32_MAX;
    SourceFault fault_ = SourceFault::none;
};
} // namespace nenenib::tests::ui
