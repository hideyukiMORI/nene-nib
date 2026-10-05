#pragma once
#include "FontFallbackRequest.hpp"
#include <optional>
#include <string>
#include <wrl/client.h>
namespace nenenib::ui::win32
{
struct FontFallbackKey
{
    std::wstring text;
    std::wstring locale_name;
    std::wstring family;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    Microsoft::WRL::ComPtr<IDWriteNumberSubstitution> substitution;
    DWRITE_READING_DIRECTION direction;
    DWRITE_FONT_WEIGHT weight;
    DWRITE_FONT_STYLE style;
    DWRITE_FONT_STRETCH stretch;
};
[[nodiscard]] std::optional<FontFallbackKey> fallback_key(const FontFallbackRequest &request);
[[nodiscard]] bool operator==(const FontFallbackKey &left, const FontFallbackKey &right) noexcept;
} // namespace nenenib::ui::win32
