#include "FontFallbackCache.hpp"
#include <algorithm>
#include <utility>
namespace nenenib::ui::win32
{
namespace
{
constexpr std::size_t maximum_entries = 128;
}

FontFallbackCache::FontFallbackCache(Microsoft::WRL::ComPtr<IDWriteFontFallback> system)
    : system_(std::move(system))
{
    entries_.reserve(maximum_entries);
}

const FontFallbackEntry *FontFallbackCache::find(const FontFallbackKey &key) const
{
    const auto found = std::ranges::find_if(entries_, [&key](const FontFallbackEntry &entry)
                                            { return entry.key == key; });
    return found == entries_.end() ? nullptr : &*found;
}

void FontFallbackCache::retain(FontFallbackEntry entry)
{
    if (entries_.size() < maximum_entries)
    {
        entries_.push_back(std::move(entry));
        return;
    }
    entries_.at(next_) = std::move(entry);
    next_ = (next_ + 1) % maximum_entries;
}

HRESULT STDMETHODCALLTYPE FontFallbackCache::MapCharacters(
    IDWriteTextAnalysisSource *source, UINT32 position, UINT32 length,
    IDWriteFontCollection *collection, const WCHAR *family, DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STYLE style, DWRITE_FONT_STRETCH stretch, UINT32 *mapped_length,
    IDWriteFont **mapped_font, FLOAT *scale) noexcept
{
    const FontFallbackRequest request{source, position, length, collection,
                                      family, weight,   style,  stretch};
    auto key = fallback_key(request);
    if (key.has_value())
    {
        const auto *entry = find(key.value());
        if (entry != nullptr)
        {
            *mapped_length = entry->length;
            *scale = entry->scale;
            return entry->font.CopyTo(mapped_font);
        }
    }
    const HRESULT result =
        system_->MapCharacters(source, position, length, collection, family, weight, style, stretch,
                               mapped_length, mapped_font, scale);
    if (result == S_OK && key.has_value())
    {
        retain(FontFallbackEntry{std::move(key.value()), *mapped_length, *mapped_font, *scale});
    }
    return result;
}
} // namespace nenenib::ui::win32
