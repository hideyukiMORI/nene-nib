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
    // 文字の位置から字形の位置を引くための DirectWrite の対応（#531）。置き換えた文字の範囲を
    // 通常は HitTestTextRange を呼ばずに run の中の横の位置へ写す（引けない行だけ
    // HitTestTextRange）。clusters が空なら対応は無い。
    UINT32 text_position;
    std::vector<UINT16> clusters;
    // advances の和（origin から右端まで）。
    FLOAT advance;
};
} // namespace nenenib::ui::win32
