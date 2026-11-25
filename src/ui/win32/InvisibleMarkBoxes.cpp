// 不可視文字の印の升（ADR 0193 の決定 10・#1977）。保持した字形 run の cluster と advance
// から引く。
#include "InvisibleMarkBoxes.hpp"

#include "BodyGlyphRun.hpp"
#include "InvisibleBox.hpp"
#include "InvisibleKind.hpp"
#include "InvisibleMark.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
using Runs = std::span<const BodyGlyphRun>;
// run ごとの送りの累積（字形 g の左端は origin.x + 累積[g]）。初めて触れた run だけ作る。
using RunSums = std::vector<std::vector<float>>;

// 位置を引ける保持か（replaced_tints と同じ条件・左から右・横倒しでない・文字の対応がある・文字の
// 位置が前へ戻らない）。
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

[[nodiscard]] const std::vector<float> &sums_of(Runs runs, RunSums &sums, std::size_t index)
{
    std::vector<float> &held = sums.at(index);
    if (held.empty())
    {
        const BodyGlyphRun &run = runs[index];
        held.reserve(run.advances.size() + 1);
        float x = 0;
        held.push_back(x);
        for (const auto advance : run.advances)
        {
            x += advance;
            held.push_back(x);
        }
    }
    return held;
}

// UTF-16 の位置 unit の字の左右の端と画面行の上下（layout の座標・保持に無ければ値なし）。
[[nodiscard]] std::optional<D2D1_RECT_F> edges_at(Runs runs, RunSums &sums, UINT32 unit,
                                                  float line_height)
{
    const auto found = std::ranges::upper_bound(runs, unit, {}, &BodyGlyphRun::text_position);
    if (found == runs.begin())
    {
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(found - runs.begin()) - 1;
    const BodyGlyphRun &run = runs[index];
    const std::size_t local = unit - run.text_position;
    if (local >= run.clusters.size())
    {
        return std::nullopt;
    }
    const std::size_t glyph = run.clusters[local];
    std::size_t after = local + 1;
    while (after < run.clusters.size() && run.clusters[after] == glyph)
    {
        ++after;
    }
    const std::size_t next = after < run.clusters.size() ? run.clusters[after] : run.indices.size();
    const std::vector<float> &held = sums_of(runs, sums, index);
    const float top = std::floor(run.origin.y / line_height) * line_height;
    return D2D1::RectF(run.origin.x + held.at(glyph), top, run.origin.x + held.at(next),
                       top + line_height);
}

// unit の字の字形が描く窓 [first, last) の中か（collector が横に画面外を外した窓・見えている字だけ
// に印を描く・#2047 のレビュー R1）。
[[nodiscard]] bool shown_at(Runs runs, UINT32 unit)
{
    const auto found = std::ranges::upper_bound(runs, unit, {}, &BodyGlyphRun::text_position);
    if (found == runs.begin())
    {
        return false;
    }
    const BodyGlyphRun &run = *(found - 1);
    const std::size_t local = unit - run.text_position;
    if (local >= run.clusters.size())
    {
        return false;
    }
    const std::size_t glyph = run.clusters[local];
    return glyph >= run.first && glyph < run.last;
}

// x の次の Tab の止まり（止まりの上にあればその次・DirectWrite の増分の Tab と同じ）。
[[nodiscard]] float next_stop(float x, float tab_stop) noexcept
{
    constexpr float settle = 0.01F;
    return (std::floor((x + settle) / tab_stop) + 1.0F) * tab_stop;
}

// 直前に置いた Tab の位置と升（続く Tab の左端に使う）。
using PlacedTab = std::pair<UINT32, D2D1_RECT_F>;

// Tab の左の字の升（行頭は幅 0 の升・続く Tab は前の Tab の升・保持に無ければ値なし）。edges は
// UTF-16 の位置 → 字の升。
template <typename Edges>
[[nodiscard]] std::optional<D2D1_RECT_F>
before_tab(Edges &edges, UINT32 unit, const std::optional<PlacedTab> &last, float line_height)
{
    if (unit == 0)
    {
        return D2D1::RectF(0, 0, 0, line_height);
    }
    if (last.has_value() && last.value().first + 1 == unit)
    {
        return last.value().second;
    }
    return edges(unit - 1);
}

// Tab の升。左端は前の字の右端、右端は後ろの字が同じ画面行にあればその左端、無ければ次の止まり。
template <typename Edges>
[[nodiscard]] D2D1_RECT_F tab_box(Edges &edges, UINT32 unit, D2D1_RECT_F before, float tab_stop)
{
    const float left = before.right;
    const auto after = edges(unit + 1);
    const float right = after.has_value() && after.value().top == before.top
                            ? after.value().left
                            : next_stop(left, tab_stop);
    return D2D1::RectF(left, before.top, std::max(left, right), before.bottom);
}
// 保持に在る字の升だけを足す（無い字は見えていない）。
void pushed(std::vector<InvisibleBox> &boxes, const std::optional<D2D1_RECT_F> &own,
            core::InvisibleKind kind)
{
    if (own.has_value())
    {
        boxes.push_back(InvisibleBox{own.value(), kind});
    }
}
} // namespace

std::optional<std::vector<InvisibleBox>> invisible_boxes(Runs runs,
                                                         std::span<const InvisibleMark> marks,
                                                         InvisibleScale scale,
                                                         std::vector<InvisibleMark> &unresolved)
{
    const auto [line_height, tab_stop] = scale;
    if (line_height <= 0 || tab_stop <= 0 || !positioned(runs))
    {
        return std::nullopt;
    }
    RunSums sums(runs.size());
    const auto edges = [runs, &sums, height = line_height](UINT32 unit)
    { return edges_at(runs, sums, unit, height); };
    std::vector<InvisibleBox> boxes;
    boxes.reserve(marks.size());
    std::optional<PlacedTab> last;
    for (const InvisibleMark &mark : marks)
    {
        if (mark.kind != core::InvisibleKind::tab)
        {
            pushed(boxes, shown_at(runs, mark.unit) ? edges(mark.unit) : std::nullopt, mark.kind);
            continue;
        }
        const auto before = before_tab(edges, mark.unit, last, line_height);
        if (!before.has_value())
        {
            // 左の字が保持に無い Tab だけを呼び手の HitTestTextRange へ回す（R2）。
            unresolved.push_back(mark);
            last.reset();
            continue;
        }
        last = PlacedTab{mark.unit, tab_box(edges, mark.unit, before.value(), tab_stop)};
        boxes.push_back(InvisibleBox{last.value().second, mark.kind});
    }
    return boxes;
}
} // namespace nenenib::ui::win32
