#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <wrl/client.h>
namespace nenenib::ui::win32
{
struct BodyGlyphRun
{
    Microsoft::WRL::ComPtr<IDWriteFontFace> face;
    FLOAT em;
    BOOL sideways;
    UINT32 bidi;
    D2D1_POINT_2F origin;
    DWRITE_MEASURING_MODE measuring;
    std::vector<UINT16> indices;
    std::vector<FLOAT> advances;
    std::vector<DWRITE_GLYPH_OFFSET> offsets;
};
} // namespace nenenib::ui::win32
