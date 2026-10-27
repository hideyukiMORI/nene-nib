#include "ForwardingFont.hpp"

#include <utility>

namespace nenenib::tests::ui
{
ForwardingFont::ForwardingFont(Microsoft::WRL::ComPtr<IDWriteFont> inner,
                               std::shared_ptr<UINT32> family_reads) noexcept
    : inner_(std::move(inner)), family_reads_(std::move(family_reads))
{
}

HRESULT STDMETHODCALLTYPE ForwardingFont::GetFontFamily(IDWriteFontFamily **family) noexcept
{
    ++*family_reads_;
    return inner_->GetFontFamily(family);
}

DWRITE_FONT_WEIGHT STDMETHODCALLTYPE ForwardingFont::GetWeight() noexcept
{
    return inner_->GetWeight();
}

DWRITE_FONT_STRETCH STDMETHODCALLTYPE ForwardingFont::GetStretch() noexcept
{
    return inner_->GetStretch();
}

DWRITE_FONT_STYLE STDMETHODCALLTYPE ForwardingFont::GetStyle() noexcept
{
    return inner_->GetStyle();
}

BOOL STDMETHODCALLTYPE ForwardingFont::IsSymbolFont() noexcept
{
    return inner_->IsSymbolFont();
}

HRESULT STDMETHODCALLTYPE ForwardingFont::GetFaceNames(IDWriteLocalizedStrings **names) noexcept
{
    return inner_->GetFaceNames(names);
}

HRESULT STDMETHODCALLTYPE ForwardingFont::GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_ID id,
                                                                  IDWriteLocalizedStrings **strings,
                                                                  BOOL *exists) noexcept
{
    return inner_->GetInformationalStrings(id, strings, exists);
}

DWRITE_FONT_SIMULATIONS STDMETHODCALLTYPE ForwardingFont::GetSimulations() noexcept
{
    return inner_->GetSimulations();
}

void STDMETHODCALLTYPE ForwardingFont::GetMetrics(DWRITE_FONT_METRICS *metrics) noexcept
{
    inner_->GetMetrics(metrics);
}

HRESULT STDMETHODCALLTYPE ForwardingFont::HasCharacter(UINT32 value, BOOL *exists) noexcept
{
    return inner_->HasCharacter(value, exists);
}

HRESULT STDMETHODCALLTYPE ForwardingFont::CreateFontFace(IDWriteFontFace **face) noexcept
{
    return inner_->CreateFontFace(face);
}
} // namespace nenenib::tests::ui
