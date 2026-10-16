#include "FontFallbackCache.hpp"
#include <utility>
namespace nenenib::ui::win32
{
FontFallbackCache::FontFallbackCache(Microsoft::WRL::ComPtr<IDWriteFontFallback> system)
    : system_(std::move(system))
{
}

// SDK-ABI: IDWriteFontFallback::MapCharacters
// NOLINTNEXTLINE(readability-function-size)
HRESULT STDMETHODCALLTYPE FontFallbackCache::MapCharacters(
    IDWriteTextAnalysisSource *source, UINT32 position, UINT32 length,
    IDWriteFontCollection *collection, const WCHAR *family, DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STYLE style, DWRITE_FONT_STRETCH stretch, UINT32 *mapped_length,
    IDWriteFont **mapped_font, FLOAT *scale) noexcept
{
    const FontFallbackRequest request{source, position, length, collection,
                                      family, weight,   style,  stretch};
    return map(request, mapped_length, mapped_font, scale);
}

HRESULT FontFallbackCache::map(const FontFallbackRequest &request, UINT32 *mapped_length,
                               IDWriteFont **mapped_font, FLOAT *scale)
{
    auto key = fallback_key(request);
    if (key.has_value())
    {
        const auto *entry = table_.find(key.value());
        if (entry != nullptr)
        {
            *mapped_length = entry->length;
            *scale = entry->scale;
            return entry->font.CopyTo(mapped_font);
        }
    }
    const HRESULT result = system_->MapCharacters(
        request.source, request.position, request.length, request.collection, request.family,
        request.weight, request.style, request.stretch, mapped_length, mapped_font, scale);
    if (result == S_OK && key.has_value())
    {
        table_.retain(
            FontFallbackEntry{std::move(key.value()), *mapped_length, *mapped_font, *scale});
    }
    return result;
}
} // namespace nenenib::ui::win32
