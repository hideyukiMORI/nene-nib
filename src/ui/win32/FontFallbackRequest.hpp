#pragma once
#include <dwrite_2.h>
namespace nenenib::ui::win32
{
// COMの借用引数を内部の処理へ渡す。呼び出しを越えて保持しない。
struct FontFallbackRequest
{
    IDWriteTextAnalysisSource *source;
    UINT32 position;
    UINT32 length;
    IDWriteFontCollection *collection;
    const WCHAR *family;
    DWRITE_FONT_WEIGHT weight;
    DWRITE_FONT_STYLE style;
    DWRITE_FONT_STRETCH stretch;
};
} // namespace nenenib::ui::win32
