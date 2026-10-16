#include "ScriptedFontFallback.hpp"

#include <algorithm>
#include <utility>

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

void ScriptedFontFallback::delegate_to(
    Microsoft::WRL::ComPtr<IDWriteFontFallback> delegate) noexcept
{
    delegate_ = std::move(delegate);
}

void ScriptedFontFallback::forget_longest() noexcept
{
    longest_ = 0;
}

UINT32 ScriptedFontFallback::longest() const noexcept
{
    return longest_;
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
    IDWriteTextAnalysisSource *source, UINT32 position, UINT32 length,
    IDWriteFontCollection *collection, const WCHAR *family, DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STYLE style, DWRITE_FONT_STRETCH stretch, UINT32 *mapped_length,
    IDWriteFont **mapped_font, FLOAT *scale) noexcept
{
    longest_ = std::max(longest_, length);
    return delegate_ == nullptr
               ? answer(length, mapped_length, mapped_font, scale)
               : delegate_->MapCharacters(source, position, length, collection, family, weight,
                                          style, stretch, mapped_length, mapped_font, scale);
}
} // namespace nenenib::tests::ui
