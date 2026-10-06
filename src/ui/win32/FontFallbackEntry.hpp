#pragma once
#include "FontFallbackKey.hpp"
namespace nenenib::ui::win32
{
struct FontFallbackEntry
{
    FontFallbackKey key;
    UINT32 length;
    Microsoft::WRL::ComPtr<IDWriteFont> font;
    FLOAT scale;
};
} // namespace nenenib::ui::win32
