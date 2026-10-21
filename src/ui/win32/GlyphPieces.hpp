#pragma once
#include "BodyGlyphRun.hpp"
#include "ColoredRun.hpp"
#include "GlyphPiece.hpp"

#include <algorithm>
#include <cstddef>
#include <d2d1.h>
#include <dwrite.h>
#include <span>
#include <vector>

namespace nenenib::ui::win32
{
// ADR 0156 の決定 11・13: 保持した字形 run を構文の色の区間の境で切る。境の UTF-16 の位置は
// run の cluster の表（text_position からの UTF-16 の単位ごとの字形の添字）で字形の添字に写し、
// 描く窓 [run.first, run.last) の中に収める（窓は collector が画面の横の範囲に狭めてある）。

// run の中の UTF-16 の位置 unit が始まる字形の添字（run の前は 0・後ろは字形の数）。
[[nodiscard]] inline std::size_t glyph_at(const BodyGlyphRun &run, UINT32 unit) noexcept
{
    if (unit <= run.text_position)
    {
        return 0;
    }
    const std::size_t local = unit - run.text_position;
    return local < run.clusters.size() ? run.clusters[local] : run.indices.size();
}

// 切れない run（cluster の表が無い・右から左）の色。run に最初に重なる区間の色（限界）。
// 表が無い run は文字の長さが判らないので、run の始まりを含む区間を見る。
[[nodiscard]] inline D2D1_COLOR_F whole_color(const BodyGlyphRun &run,
                                              std::span<const ColoredRun> colored,
                                              const D2D1_COLOR_F &plain) noexcept
{
    const UINT32 begin = run.text_position;
    const UINT32 end = begin + static_cast<UINT32>(std::max<std::size_t>(run.clusters.size(), 1));
    for (const auto &span : colored)
    {
        if (span.range.startPosition < end && begin < span.range.startPosition + span.range.length)
        {
            return span.color;
        }
    }
    return plain;
}

// 窓の部分を色ごとに前から並べる。区間の外は plain の色。空の部分は並べない。
[[nodiscard]] inline std::vector<GlyphPiece>
glyph_pieces(const BodyGlyphRun &run, std::span<const ColoredRun> colored,
             const D2D1_COLOR_F &plain)
{
    std::vector<GlyphPiece> pieces;
    if (run.first >= run.last)
    {
        return pieces;
    }
    if (run.clusters.empty() || run.bidi % 2 != 0)
    {
        pieces.push_back(GlyphPiece{run.first, run.last, whole_color(run, colored, plain)});
        return pieces;
    }
    std::size_t at = run.first;
    for (const auto &span : colored)
    {
        const auto from = std::clamp(glyph_at(run, span.range.startPosition), at, run.last);
        const auto to =
            std::clamp(glyph_at(run, span.range.startPosition + span.range.length), from, run.last);
        if (from > at)
        {
            pieces.push_back(GlyphPiece{at, from, plain});
        }
        if (to > from)
        {
            pieces.push_back(GlyphPiece{from, to, span.color});
        }
        at = std::max(at, to);
    }
    if (at < run.last)
    {
        pieces.push_back(GlyphPiece{at, run.last, plain});
    }
    return pieces;
}
} // namespace nenenib::ui::win32
