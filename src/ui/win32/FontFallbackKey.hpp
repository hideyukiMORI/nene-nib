#pragma once
#include "FontFallbackRequest.hpp"
#include <cstddef>
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
    // 上の全欄の hash。保持の表の索引の入口で、一致は全欄の比較で決める（#586）。
    std::size_t hash;
};
[[nodiscard]] std::optional<FontFallbackKey> fallback_key(const FontFallbackRequest &request);
// key の hash 以外の全欄から作る hash。
[[nodiscard]] std::size_t fallback_hash(const FontFallbackKey &key) noexcept;
[[nodiscard]] bool operator==(const FontFallbackKey &left, const FontFallbackKey &right) noexcept;
} // namespace nenenib::ui::win32
