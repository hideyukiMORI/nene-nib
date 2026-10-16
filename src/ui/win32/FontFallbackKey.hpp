#pragma once
#include "FontFallbackRequest.hpp"
#include <optional>
#include <string>
#include <wrl/client.h>
namespace nenenib::ui::win32
{
// 保持の鍵にする本文の長さの上限（UTF-16 単位・ADR 0071）。字体選択へ渡す区切りの長さも同じ上限で
// 切る（ADR 0142 の決定 3）。
inline constexpr UINT32 fallback_key_text_limit = 256;
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
