#include "ReplacedTints.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
using Runs = std::span<const BodyGlyphRun>;

[[nodiscard]] bool positioned(Runs runs)
{
    UINT32 reached = 0;
    for (const auto &run : runs)
    {
        if (run.bidi % 2 != 0 || run.sideways != FALSE || run.clusters.empty() ||
            run.text_position < reached)
        {
            return false;
        }
        reached = run.text_position + static_cast<UINT32>(run.clusters.size());
    }
    return true;
}

[[nodiscard]] float row_of(const BodyGlyphRun &run, float line_height)
{
    return std::floor(run.origin.y / line_height);
}

// 切り抜きと横に交わる run の区間。保持は行ごとに左から右の順なので、自分の run から左右へ
// 交わらなくなるまで広げる（同じ行の隣だけを見る）。
[[nodiscard]] ReplacedTint crossing(Runs runs, std::size_t own, D2D1_RECT_F clip, float line_height)
{
    const float row = row_of(runs[own], line_height);
    std::size_t first = own;
    while (first > 0 && row_of(runs[first - 1], line_height) == row &&
           runs[first - 1].origin.x + runs[first - 1].advance > clip.left)
    {
        --first;
    }
    std::size_t last = own + 1;
    while (last < runs.size() && row_of(runs[last], line_height) == row &&
           runs[last].origin.x < clip.right)
    {
        ++last;
    }
    return ReplacedTint{clip, first, last};
}

// 字形の番号と、そこまでの advances の和。
using Position = std::pair<std::size_t, float>;

// glyph まで進めた位置。範囲は前から順なので、run ごとにほぼ 1 回の走査で済む。
[[nodiscard]] Position walked(const BodyGlyphRun &run, Position from, std::size_t glyph)
{
    if (glyph < from.first)
    {
        from = Position{0, 0.0F};
    }
    while (from.first < glyph)
    {
        from.second += run.advances.at(from.first);
        ++from.first;
    }
    return from;
}

[[nodiscard]] UINT32 end_of(DWRITE_TEXT_RANGE range)
{
    return range.startPosition + range.length;
}

// range が run に重なる部分の字形の区間 [first, last)。文字の塊（cluster）ごと含める。
[[nodiscard]] std::pair<std::size_t, std::size_t> glyphs_of(const BodyGlyphRun &run,
                                                            DWRITE_TEXT_RANGE range)
{
    const auto size = static_cast<UINT32>(run.clusters.size());
    const UINT32 begin = run.text_position;
    const UINT32 from = std::max(range.startPosition, begin) - begin;
    const UINT32 to = std::min(end_of(range), begin + size) - begin;
    return {run.clusters.at(from), to < size ? run.clusters.at(to) : run.indices.size()};
}

// 1 つの run に重なる範囲（ranges の先頭から、run の終わりより前に始まるもの）の切り抜き。
[[nodiscard]] std::vector<ReplacedTint>
run_tints(Runs runs, std::size_t own, std::span<const DWRITE_TEXT_RANGE> ranges, float line_height)
{
    const auto &run = runs[own];
    const UINT32 end = run.text_position + static_cast<UINT32>(run.clusters.size());
    const float top = row_of(run, line_height) * line_height;
    std::vector<ReplacedTint> tints;
    Position at{0, 0.0F};
    for (const auto &range : ranges)
    {
        if (range.startPosition >= end)
        {
            break;
        }
        const auto [first, last] = glyphs_of(run, range);
        at = walked(run, at, first);
        const float left = run.origin.x + at.second;
        at = walked(run, at, last);
        const float right = run.origin.x + at.second;
        if (right > left)
        {
            tints.push_back(
                crossing(runs, own, D2D1::RectF(left, top, right, top + line_height), line_height));
        }
    }
    return tints;
}
} // namespace

std::optional<std::vector<ReplacedTint>> replaced_tints(std::span<const BodyGlyphRun> runs,
                                                        std::span<const DWRITE_TEXT_RANGE> ranges,
                                                        float line_height)
{
    const auto before = [](DWRITE_TEXT_RANGE left, DWRITE_TEXT_RANGE right)
    { return left.startPosition < right.startPosition; };
    if (line_height <= 0 || !positioned(runs) || !std::ranges::is_sorted(ranges, before))
    {
        return std::nullopt;
    }
    std::vector<ReplacedTint> tints;
    std::size_t next = 0;
    for (std::size_t own = 0; own < runs.size(); ++own)
    {
        while (next < ranges.size() && end_of(ranges[next]) <= runs[own].text_position)
        {
            ++next;
        }
        const auto added = run_tints(runs, own, ranges.subspan(next), line_height);
        tints.insert(tints.end(), added.begin(), added.end());
    }
    return tints;
}
} // namespace nenenib::ui::win32
