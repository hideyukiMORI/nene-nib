#include "ScriptedFontFallback.hpp"

namespace nenenib::tests::ui
{
void ScriptedFontFallback::set_result(HRESULT result) noexcept
{
    result_ = result;
}

HRESULT ScriptedFontFallback::result() const noexcept
{
    return result_;
}

UINT32 ScriptedFontFallback::calls() const noexcept
{
    return calls_;
}

HRESULT ScriptedFontFallback::answer(UINT32 length, UINT32 *mapped_length,
                                     IDWriteFont **mapped_font, FLOAT *scale)
{
    ++calls_;
    *mapped_length = length;
    *mapped_font = nullptr;
    *scale = static_cast<FLOAT>(calls_);
    return result_;
}

// SDK-ABI: IDWriteFontFallback::MapCharacters
// NOLINTNEXTLINE(readability-function-size)
HRESULT STDMETHODCALLTYPE ScriptedFontFallback::MapCharacters(
    IDWriteTextAnalysisSource *, UINT32, UINT32 length, IDWriteFontCollection *, const WCHAR *,
    DWRITE_FONT_WEIGHT, DWRITE_FONT_STYLE, DWRITE_FONT_STRETCH, UINT32 *mapped_length,
    IDWriteFont **mapped_font, FLOAT *scale) noexcept
{
    return answer(length, mapped_length, mapped_font, scale);
}
} // namespace nenenib::tests::ui
