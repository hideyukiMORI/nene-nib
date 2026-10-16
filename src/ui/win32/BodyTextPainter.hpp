#pragma once
#include "BodyGlyphRun.hpp"
#include "BodyTextLayout.hpp"
#include "BodyTextLayouts.hpp"
#include "LayoutRect.hpp"
#include "RenderFailure.hpp"
#include "ReplacedTint.hpp"
#include "ReplacedTints.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
// 本文の layout を保持した字形で描く経路（ADR 0073）と、その上に範囲の字を描き直す経路
// （tint_runs と置き換えの draw_replaced・ADR 0040 の決定 4・#531）を 1 つに持つ。
// Target は製品では Direct2DRenderer の device context（ID2D1RenderTarget）で、tests/ui は
// 描画の呼び出しを数える替え玉を渡す（#531）。Layouts は BodyTextLayouts の glyphs_of を持つ
// 文字組みで、格子の GridRowPainter も字形を描くときはこの経路を借りる（ADR 0140 の決定 5・#569）。
// 色は呼び手が brush に置いてから呼ぶ。
template <typename Target, typename Layouts = BodyTextLayouts> class BodyTextPainter final
{
  public:
    BodyTextPainter(Target &target, ID2D1Brush *brush, Layouts &layouts,
                    std::optional<RenderFailure> &failure)
        : target_(target), brush_(brush), layouts_(layouts), failure_(failure)
    {
    }
    // 本文を描いている間の描く範囲の縦（本文の上端・下端の物理画素）。外では限りなし（#528）。
    void set_rows(std::int32_t top, std::int32_t bottom) noexcept
    {
        top_ = top;
        bottom_ = bottom;
    }
    // 行の layout の保持した字形。描く範囲の縦横は draw_body_text と同じ（#528）。
    [[nodiscard]] const BodyTextLayout *glyph_entry(IDWriteTextLayout *text,
                                                    const core::LayoutRect &area)
    {
        const auto left =
            std::max(static_cast<float>(core::width_of(area)) - text->GetMaxWidth(), 0.0F);
        const auto [top, bottom] = drawn_rows(area);
        return layouts_.glyphs_of(text, left, top, bottom);
    }
    // 保持した字形で描いた画素が DrawTextLayout と一致するのは、
    // 次の 3 つがそろうときだけ（ADR 0073）。
    // (1) 描画先の DPI が core::reference_dpi（96）で、collector の GetPixelsPerDip が 1。
    // (2) 描画先の変換が恒等で、collector の GetCurrentTransform も恒等。
    // (3) 行の原点 area.left / area.top が整数の画素。
    // どれかを変えるときは字形の保持をやめるか、collector の答えを同じ値から出す。
    // tests/ui が (1) と (2) の collector 側の値を確かめる。
    // 保持の run は文字の位置・cluster・advance も持ち、置き換えの描画は本文と同じ鍵
    // （left / top / bottom）の entry を引く（#531）。
    void draw_body_text(IDWriteTextLayout *text, const core::LayoutRect &area,
                        D2D1_DRAW_TEXT_OPTIONS options)
    {
        const auto *entry = glyph_entry(text, area);
        const auto origin =
            D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top));
        if (entry == nullptr || !entry->glyphs_ready)
        {
            target_.PushAxisAlignedClip(to_clip(area), D2D1_ANTIALIAS_MODE_ALIASED);
            target_.DrawTextLayout(origin, text, brush_, D2D1_DRAW_TEXT_OPTIONS_NONE);
            target_.PopAxisAlignedClip();
            return;
        }
        if (options == D2D1_DRAW_TEXT_OPTIONS_CLIP)
        {
            target_.PushAxisAlignedClip(to_clip(area), D2D1_ANTIALIAS_MODE_ALIASED);
        }
        draw_glyph_runs(entry->glyphs, origin);
        if (options == D2D1_DRAW_TEXT_OPTIONS_CLIP)
        {
            target_.PopAxisAlignedClip();
        }
    }
    // 折り返し・双方向の全矩形。固定数の上限で選択やIMEの下線を切らない。
    [[nodiscard]] std::vector<DWRITE_HIT_TEST_METRICS>
    runs_of(IDWriteTextLayout *text, const core::LayoutRect &area, DWRITE_TEXT_RANGE range)
    {
        if (range.length == 0)
        {
            return {};
        }
        UINT32 count = 0;
        const auto result =
            text->HitTestTextRange(range.startPosition, range.length, static_cast<float>(area.left),
                                   static_cast<float>(area.top), nullptr, 0, &count);
        if (FAILED(result) && result != E_NOT_SUFFICIENT_BUFFER)
        {
            failure_ = RenderFailure::directwrite;
            return {};
        }
        std::vector<DWRITE_HIT_TEST_METRICS> runs(count);
        if (FAILED(text->HitTestTextRange(
                range.startPosition, range.length, static_cast<float>(area.left),
                static_cast<float>(area.top), runs.data(), count, &count)))
        {
            failure_ = RenderFailure::directwrite;
            return {};
        }
        runs.resize(count);
        return runs;
    }
    // 範囲の字だけを描き直す。範囲の当たり矩形ごとに、その切り抜きの中に行の layout
    // をもう一度通す。
    void tint_runs(IDWriteTextLayout *text, const core::LayoutRect &area, DWRITE_TEXT_RANGE range)
    {
        for (const auto &run : runs_of(text, area, range))
        {
            target_.PushAxisAlignedClip(
                D2D1::RectF(run.left, run.top, run.left + run.width, run.top + run.height),
                D2D1_ANTIALIAS_MODE_ALIASED);
            draw_body_text(text, area, D2D1_DRAW_TEXT_OPTIONS_NONE);
            target_.PopAxisAlignedClip();
        }
    }
    // 置き換えた文字（`^M`・`<200b>`）の描き直し（ADR 0040 の決定 4）。ranges は描いた layout の
    // UTF-16 の範囲で前から順。切り抜きは保持した字形 run から引き、交わる run だけを描く（#531）。
    // 範囲ごとに tint_runs（HitTestTextRange と行の全 run）を通すと置き換えの数 × 行の長さになり、
    // 1 行の長い文書で止まる。保持で位置を引けない行だけ tint_runs に戻る。
    void draw_replaced(IDWriteTextLayout *text, const core::LayoutRect &area,
                       std::span<const DWRITE_TEXT_RANGE> ranges)
    {
        const auto *entry = glyph_entry(text, area);
        const auto tints = glyph_tints(entry, text, ranges);
        if (!tints.has_value())
        {
            for (const auto &range : shown_ranges(text, area, ranges))
            {
                tint_runs(text, area, range);
            }
            return;
        }
        const auto origin =
            D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top));
        const std::span<const BodyGlyphRun> runs = entry->glyphs;
        for (const auto &tint : tints.value())
        {
            target_.PushAxisAlignedClip(
                D2D1::RectF(origin.x + tint.clip.left, origin.y + tint.clip.top,
                            origin.x + tint.clip.right, origin.y + tint.clip.bottom),
                D2D1_ANTIALIAS_MODE_ALIASED);
            draw_glyph_runs(runs.subspan(tint.first, tint.last - tint.first), origin);
            target_.PopAxisAlignedClip();
        }
    }

  private:
    [[nodiscard]] static D2D1_RECT_F to_clip(const core::LayoutRect &area) noexcept
    {
        return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                           static_cast<float>(area.right), static_cast<float>(area.bottom));
    }
    // 描く範囲の縦を layout の座標へ写す。行の全体が範囲に入るなら限りなしにして、
    // 普通の行の字形の保持を今までどおり使い回す（#528・ADR 0073 の保守的な除外に縦を足す）。
    [[nodiscard]] std::pair<float, float> drawn_rows(const core::LayoutRect &area) const noexcept
    {
        const auto from = static_cast<float>(top_) - static_cast<float>(area.top);
        const auto to = static_cast<float>(bottom_) - static_cast<float>(area.top);
        if (from <= 0 && to >= static_cast<float>(core::height_of(area)))
        {
            return {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max()};
        }
        return {from, to};
    }
    void draw_glyph_runs(std::span<const BodyGlyphRun> runs, D2D1_POINT_2F origin)
    {
        for (const auto &stored : runs)
        {
            const DWRITE_GLYPH_RUN run = drawn_glyphs(stored);
            target_.DrawGlyphRun(drawn_origin(stored, origin), &run, brush_, stored.measuring);
        }
    }
    // 保持した字形から引いた置き換えの切り抜き。引けなければ nullopt（#531）。
    [[nodiscard]] static std::optional<std::vector<ReplacedTint>>
    glyph_tints(const BodyTextLayout *entry, IDWriteTextLayout *text,
                std::span<const DWRITE_TEXT_RANGE> ranges)
    {
        DWRITE_LINE_SPACING_METHOD method{};
        float spacing = 0;
        float baseline = 0;
        if (entry == nullptr || !entry->glyphs_ready ||
            FAILED(text->GetLineSpacing(&method, &spacing, &baseline)) ||
            method != DWRITE_LINE_SPACING_METHOD_UNIFORM)
        {
            return std::nullopt;
        }
        return replaced_tints(entry->glyphs, ranges, spacing);
    }
    // 保持で位置を引けない行（収集の棄却・右から左など）は HitTestTextRange に戻る。置き換えが
    // replaced_hit_test_limit 個を超える行では、見えている文字の範囲（描く範囲の左上と右下の
    // HitTestPoint）に重なる置き換えだけにする。HitTestTextRange 1 回は layout の大きさに比例する
    // ので、全部に呼ぶと置き換えの数 × 行の長さになる（#531）。上限までは今までどおり全部を描く。
    [[nodiscard]] std::vector<DWRITE_TEXT_RANGE>
    shown_ranges(IDWriteTextLayout *text, const core::LayoutRect &area,
                 std::span<const DWRITE_TEXT_RANGE> ranges)
    {
        if (ranges.size() <= replaced_hit_test_limit)
        {
            return {ranges.begin(), ranges.end()};
        }
        const auto left =
            std::max(static_cast<float>(core::width_of(area)) - text->GetMaxWidth(), 0.0F);
        const auto [top, bottom] = drawn_rows(area);
        BOOL trailing = FALSE;
        BOOL inside = FALSE;
        DWRITE_HIT_TEST_METRICS first{};
        DWRITE_HIT_TEST_METRICS last{};
        if (FAILED(text->HitTestPoint(left, std::max(top, 0.0F), &trailing, &inside, &first)) ||
            FAILED(text->HitTestPoint(left + text->GetMaxWidth(),
                                      std::min(bottom, static_cast<float>(core::height_of(area))),
                                      &trailing, &inside, &last)))
        {
            failure_ = RenderFailure::directwrite;
            return {};
        }
        std::vector<DWRITE_TEXT_RANGE> shown;
        for (const auto &range : ranges)
        {
            if (range.startPosition + range.length > first.textPosition &&
                range.startPosition <= last.textPosition + last.length)
            {
                shown.push_back(range);
            }
        }
        return shown;
    }
    // 1 行の置き換えが多いときだけ見えている範囲に絞る。普通の行（`^M` が数個）は今までどおり
    // 全部を描き、絵を変えない。1,000 は目安（未測定）で、1 行の HitTestTextRange 1,000 回の
    // 時間はまだ測っていない。
    static constexpr std::size_t replaced_hit_test_limit = 1000;

    Target &target_;
    ID2D1Brush *brush_;
    Layouts &layouts_;
    std::optional<RenderFailure> &failure_;
    std::int32_t top_ = std::numeric_limits<std::int32_t>::min();
    std::int32_t bottom_ = std::numeric_limits<std::int32_t>::max();
};
} // namespace nenenib::ui::win32
