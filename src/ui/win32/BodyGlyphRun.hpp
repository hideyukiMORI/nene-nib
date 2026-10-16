#pragma once
#include <cstddef>
#include <d2d1.h>
#include <dwrite.h>
#include <span>
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
    // 描く字形の窓 [first, last)。字形の配列は全体を持ち、横に画面と交わる run だけ窓が狭い
    // （ADR 0073 の 2026-10-16 の補足）。描くのは drawn_glyphs と drawn_origin の 1 対だけ。
    std::size_t first;
    std::size_t last;
};

// 窓の字形を DrawGlyphRun へ渡す形。offsets も同じ位置から渡す。
[[nodiscard]] inline DWRITE_GLYPH_RUN drawn_glyphs(const BodyGlyphRun &run)
{
    const auto count = run.last - run.first;
    return DWRITE_GLYPH_RUN{
        run.face.Get(),
        run.em,
        static_cast<UINT32>(count),
        std::span(run.indices).subspan(run.first, count).data(),
        std::span(run.advances).subspan(run.first, count).data(),
        run.offsets.empty() ? nullptr : std::span(run.offsets).subspan(run.first, count).data(),
        run.sideways,
        run.bidi};
}

// 窓の先頭の字形の原点。描画の原点（origin + run.origin）から窓の前の送りを左から float で積む。
// Direct2D が run の中で積むのと同じ順なので、全体を描いたときと同じ位置になる（画素は同じ）。
[[nodiscard]] inline D2D1_POINT_2F drawn_origin(const BodyGlyphRun &run, D2D1_POINT_2F origin)
{
    float x = origin.x + run.origin.x;
    for (const auto advance : std::span(run.advances).first(run.first))
    {
        x += advance;
    }
    return D2D1::Point2F(x, origin.y + run.origin.y);
}
} // namespace nenenib::ui::win32
