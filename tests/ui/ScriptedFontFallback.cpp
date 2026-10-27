#include "ScriptedFontFallback.hpp"

#include "ForwardingFont.hpp"

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

void ScriptedFontFallback::answer_with(Microsoft::WRL::ComPtr<IDWriteFont> font) noexcept
{
    answer_font_ = std::move(font);
}

void ScriptedFontFallback::wrap_fonts() noexcept
{
    wraps_ = true;
}

UINT32 ScriptedFontFallback::family_reads() const noexcept
{
    return *family_reads_;
}

// 呼ばれた回数と渡された長さの最大を数える（委ね先がある呼び出しも数える）。
void ScriptedFontFallback::counted(UINT32 length) noexcept
{
    ++calls_;
    longest_ = std::max(longest_, length);
}

// 包むなら、委ね先か替え玉が返した字体を新しい ForwardingFont
// に包み替える（所有は呼び出し元へ渡す）。
HRESULT ScriptedFontFallback::wrapped(HRESULT result, IDWriteFont **mapped_font) noexcept
{
    if (!wraps_ || FAILED(result) || mapped_font == nullptr || *mapped_font == nullptr)
    {
        return result;
    }
    Microsoft::WRL::ComPtr<IDWriteFont> inner;
    inner.Attach(*mapped_font);
    const auto wrapped = Microsoft::WRL::Make<ForwardingFont>(inner, family_reads_);
    *mapped_font = wrapped != nullptr ? wrapped.Get() : inner.Get();
    (*mapped_font)->AddRef();
    return result;
}

HRESULT ScriptedFontFallback::answer(UINT32 length, UINT32 *mapped_length,
                                     IDWriteFont **mapped_font, FLOAT *scale)
{
    *mapped_length = length;
    *mapped_font = answer_font_.Get();
    if (answer_font_ != nullptr)
    {
        answer_font_->AddRef();
    }
    *scale = answer_font_ != nullptr ? 1.0F : static_cast<FLOAT>(calls_);
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
    counted(length);
    return wrapped(delegate_ == nullptr ? answer(length, mapped_length, mapped_font, scale)
                                        : delegate_->MapCharacters(
                                              source, position, length, collection, family, weight,
                                              style, stretch, mapped_length, mapped_font, scale),
                   mapped_font);
}
} // namespace nenenib::tests::ui
