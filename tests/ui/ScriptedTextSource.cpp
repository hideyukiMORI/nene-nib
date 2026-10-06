#include "ScriptedTextSource.hpp"

#include <algorithm>
#include <dwrite_1.h>
#include <utility>

namespace nenenib::tests::ui
{
void ScriptedTextSource::set_text(std::wstring text)
{
    text_ = std::move(text);
}

void ScriptedTextSource::set_language(std::wstring language)
{
    language_ = std::move(language);
}

void ScriptedTextSource::set_substitution(
    Microsoft::WRL::ComPtr<IDWriteNumberSubstitution> substitution)
{
    substitution_ = std::move(substitution);
}

void ScriptedTextSource::set_direction(DWRITE_READING_DIRECTION direction) noexcept
{
    direction_ = direction;
}

void ScriptedTextSource::set_property_lengths(UINT32 language, UINT32 substitution) noexcept
{
    language_limit_ = language;
    substitution_limit_ = substitution;
}

void ScriptedTextSource::set_fault(SourceFault fault) noexcept
{
    fault_ = fault;
}

UINT32 ScriptedTextSource::length() const noexcept
{
    return static_cast<UINT32>(text_.size());
}

UINT32 ScriptedTextSource::bounded(UINT32 limit) const noexcept
{
    return std::min(limit, length());
}

HRESULT STDMETHODCALLTYPE ScriptedTextSource::QueryInterface(REFIID interface_id,
                                                             void **object) noexcept
{
    if (fault_ == SourceFault::broken_extension &&
        interface_id == __uuidof(IDWriteTextAnalysisSource1))
    {
        *object = nullptr;
        return E_FAIL;
    }
    return RuntimeClass::QueryInterface(interface_id, object);
}

HRESULT STDMETHODCALLTYPE ScriptedTextSource::GetTextAtPosition(UINT32 position, const WCHAR **text,
                                                                UINT32 *length) noexcept
{
    if (fault_ == SourceFault::broken_text)
    {
        return E_FAIL;
    }
    if (fault_ == SourceFault::hidden_tail && position == this->length())
    {
        *text = L"x";
        *length = 1;
        return S_OK;
    }
    *length = position < this->length() ? this->length() - position : 0;
    *text = *length != 0 ? &text_.at(position) : nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ScriptedTextSource::GetTextBeforePosition(UINT32 position,
                                                                    const WCHAR **text,
                                                                    UINT32 *length) noexcept
{
    if (fault_ == SourceFault::hidden_prefix && position == 0)
    {
        *text = L"x";
        *length = 1;
        return S_OK;
    }
    *length = bounded(position);
    *text = *length != 0 ? text_.data() : nullptr;
    return S_OK;
}

DWRITE_READING_DIRECTION STDMETHODCALLTYPE
ScriptedTextSource::GetParagraphReadingDirection() noexcept
{
    return direction_;
}

HRESULT STDMETHODCALLTYPE ScriptedTextSource::GetLocaleName(UINT32, UINT32 *length,
                                                            const WCHAR **name) noexcept
{
    *length = bounded(language_limit_);
    *name = language_.c_str();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ScriptedTextSource::GetNumberSubstitution(
    UINT32, UINT32 *length, IDWriteNumberSubstitution **substitution) noexcept
{
    *length = bounded(substitution_limit_);
    return substitution_.CopyTo(substitution);
}
} // namespace nenenib::tests::ui
