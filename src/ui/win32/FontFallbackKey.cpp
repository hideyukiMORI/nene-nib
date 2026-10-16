#include "FontFallbackKey.hpp"
#include <bit>
#include <cstdint>
#include <dwrite_1.h>
#include <functional>
#include <string_view>
namespace nenenib::ui::win32
{
namespace
{
constexpr UINT32 maximum_text = 256;
constexpr std::size_t maximum_name = 128;

bool bounded_name(const WCHAR *name)
{
    if (name == nullptr)
    {
        return false;
    }
    for (std::size_t index = 0; index <= maximum_name; ++index)
    {
        if (name[index] == L'\0')
        {
            return true;
        }
    }
    return false;
}

// boost の hash_combine と同じ混ぜ方。
std::size_t mixed(std::size_t hash, std::size_t value) noexcept
{
    constexpr std::size_t golden = 0x9e3779b97f4a7c15U;
    return hash ^ (value + golden + (hash << 6U) + (hash >> 2U));
}

bool complete_source(const FontFallbackRequest &request)
{
    Microsoft::WRL::ComPtr<IDWriteTextAnalysisSource1> extended;
    if (request.source->QueryInterface(IID_PPV_ARGS(&extended)) != E_NOINTERFACE)
    {
        return false;
    }
    const WCHAR *outside = nullptr;
    UINT32 length = 1;
    return SUCCEEDED(request.source->GetTextBeforePosition(0, &outside, &length)) && length == 0 &&
           SUCCEEDED(request.source->GetTextAtPosition(request.length, &outside, &length)) &&
           length == 0;
}

bool source_properties(const FontFallbackRequest &request, FontFallbackKey &key)
{
    const WCHAR *locale_name = nullptr;
    UINT32 length = 0;
    if (FAILED(request.source->GetLocaleName(0, &length, &locale_name)) ||
        length < request.length || !bounded_name(locale_name))
    {
        return false;
    }
    key.locale_name = locale_name;
    if (FAILED(request.source->GetNumberSubstitution(0, &length, &key.substitution)) ||
        length < request.length)
    {
        return false;
    }
    key.direction = request.source->GetParagraphReadingDirection();
    return true;
}
} // namespace

std::optional<FontFallbackKey> fallback_key(const FontFallbackRequest &request)
{
    if (request.position != 0 || request.length == 0 || request.length > maximum_text ||
        !bounded_name(request.family))
    {
        return std::nullopt;
    }
    const WCHAR *text = nullptr;
    UINT32 length = 0;
    if (FAILED(request.source->GetTextAtPosition(0, &text, &length)) || length != request.length ||
        text == nullptr || !complete_source(request))
    {
        return std::nullopt;
    }
    FontFallbackKey key{std::wstring(text, length),
                        {},
                        request.family,
                        request.collection,
                        {},
                        DWRITE_READING_DIRECTION_LEFT_TO_RIGHT,
                        request.weight,
                        request.style,
                        request.stretch,
                        0};
    if (!source_properties(request, key))
    {
        return std::nullopt;
    }
    key.hash = fallback_hash(key);
    return key;
}

std::size_t fallback_hash(const FontFallbackKey &key) noexcept
{
    const std::hash<std::wstring_view> text_hash;
    std::size_t hash = text_hash(key.text);
    hash = mixed(hash, text_hash(key.locale_name));
    hash = mixed(hash, text_hash(key.family));
    hash = mixed(hash, std::bit_cast<std::uintptr_t>(key.collection.Get()));
    hash = mixed(hash, std::bit_cast<std::uintptr_t>(key.substitution.Get()));
    hash = mixed(hash, static_cast<std::size_t>(key.direction));
    hash = mixed(hash, static_cast<std::size_t>(key.weight));
    hash = mixed(hash, static_cast<std::size_t>(key.style));
    return mixed(hash, static_cast<std::size_t>(key.stretch));
}

bool operator==(const FontFallbackKey &left, const FontFallbackKey &right) noexcept
{
    return left.hash == right.hash && left.text == right.text &&
           left.locale_name == right.locale_name && left.family == right.family &&
           left.collection.Get() == right.collection.Get() &&
           left.substitution.Get() == right.substitution.Get() &&
           left.direction == right.direction && left.weight == right.weight &&
           left.style == right.style && left.stretch == right.stretch;
}
} // namespace nenenib::ui::win32
