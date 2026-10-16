#include "FallbackRunSource.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::ui::win32
{
FallbackRunSource::FallbackRunSource(std::wstring language, DWRITE_READING_DIRECTION direction)
    : language_(std::move(language)), direction_(direction)
{
}

void FallbackRunSource::reset(std::wstring_view text)
{
    text_.assign(text);
}

UINT32 FallbackRunSource::length() const noexcept
{
    return static_cast<UINT32>(text_.size());
}

UINT32 FallbackRunSource::after(UINT32 position) const noexcept
{
    return position < length() ? length() - position : 0;
}

HRESULT STDMETHODCALLTYPE FallbackRunSource::GetTextAtPosition(UINT32 position, const WCHAR **text,
                                                               UINT32 *length) noexcept
{
    *length = after(position);
    *text = *length != 0 ? &text_.at(position) : nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE FallbackRunSource::GetTextBeforePosition(UINT32 position,
                                                                   const WCHAR **text,
                                                                   UINT32 *length) noexcept
{
    *length = std::min(position, this->length());
    *text = *length != 0 ? text_.data() : nullptr;
    return S_OK;
}

DWRITE_READING_DIRECTION STDMETHODCALLTYPE
FallbackRunSource::GetParagraphReadingDirection() noexcept
{
    return direction_;
}

HRESULT STDMETHODCALLTYPE FallbackRunSource::GetLocaleName(UINT32 position, UINT32 *length,
                                                           const WCHAR **name) noexcept
{
    *length = after(position);
    *name = language_.c_str();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE FallbackRunSource::GetNumberSubstitution(
    UINT32 position, UINT32 *length, IDWriteNumberSubstitution **substitution) noexcept
{
    *length = after(position);
    *substitution = nullptr;
    return S_OK;
}
} // namespace nenenib::ui::win32
