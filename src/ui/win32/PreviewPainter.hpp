#pragma once

#include "LayoutRect.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewInk.hpp"
#include "PreviewPlacement.hpp"
#include "PreviewScene.hpp"
#include "PreviewStyle.hpp"
#include "PreviewTextLayout.hpp"
#include "PreviewTextLayouts.hpp"
#include "RenderFailure.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
// Markdown プレビューを描く経路（ADR 0151 の決定 17・採用案 D62 の案 B）。上から高さを足した
// 部分和（PreviewBlockTops・錨の要素から枠の高さまで）で見えている要素だけを DrawTextLayout
// で描き、面と線は FillRectangle / FillRoundedRectangle。Target は製品では Direct2D の描画先
// （ID2D1RenderTarget）で、tests/ui は呼び出しを数える替え玉、tests/preview は WIC の bitmap
// の描画先を渡す。色は役割（PreviewInk）→ PreviewBrushes の brush だけで、色の値を書かない。
template <typename Target> class PreviewPainter final
{
  public:
    PreviewPainter(Target &target, PreviewTextLayouts &layouts, const PreviewBrushes &brushes,
                   std::optional<RenderFailure> &failure)
        : target_(target), layouts_(layouts), brushes_(brushes), failure_(failure)
    {
    }
    // 枠を地の色で塗り、枠の中で見えている要素を描く。失敗は failure に置いて止まる。
    void paint(const PreviewScene &scene)
    {
        const auto frame = rect_of(scene.frame);
        target_.FillRectangle(frame, brushes_.of(PreviewInk::background));
        target_.PushAxisAlignedClip(frame, D2D1_ANTIALIAS_MODE_ALIASED);
        const auto height = static_cast<float>(core::height_of(scene.frame));
        for (auto index = scene.tops.first_visible(scene.scroll);
             index < scene.tops.size() && scene.tops.top(index) - scene.scroll < height &&
             !failure_.has_value();
             ++index)
        {
            draw_block(scene, index);
        }
        target_.PopAxisAlignedClip();
    }

  private:
    static constexpr float inline_padding_dips = 4.0F;
    static constexpr float inline_radius_dips = 3.0F;
    static constexpr float code_radius_dips = 5.0F;
    static constexpr float quote_rule_dips = 2.0F;
    // 行内コードの面は行高（本文の 1.6 倍）の上下を 1 割ずつ空ける。
    static constexpr float inline_inset_ratio = 0.1F;

    [[nodiscard]] static D2D1_RECT_F rect_of(const core::LayoutRect &area) noexcept
    {
        return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                           static_cast<float>(area.right), static_cast<float>(area.bottom));
    }

    // 要素の文字の色（見出しは keyword・コードは string・HTML と水平線は comment・引用の中の
    // 段落と項目は comment・ほかは foreground）。
    [[nodiscard]] static PreviewInk ink_of(const core::PreviewBlock &block) noexcept
    {
        switch (block.kind)
        {
        case core::PreviewBlockKind::heading:
            return PreviewInk::keyword;
        case core::PreviewBlockKind::code:
            return PreviewInk::string;
        case core::PreviewBlockKind::html:
        case core::PreviewBlockKind::rule:
            return PreviewInk::comment;
        case core::PreviewBlockKind::paragraph:
        case core::PreviewBlockKind::list_item:
            return block.quote_depth > 0 ? PreviewInk::comment : PreviewInk::foreground;
        }
        std::unreachable();
    }

    void draw_block(const PreviewScene &scene, std::size_t index)
    {
        const auto &block = scene.document.blocks.at(index);
        const auto origin = D2D1::Point2F(
            static_cast<float>(scene.frame.left) + scene.tops.indent(index),
            static_cast<float>(scene.frame.top) + scene.tops.top(index) - scene.scroll);
        draw_quote_rules(scene, index);
        switch (block.kind)
        {
        case core::PreviewBlockKind::rule:
            draw_rule(scene, origin.y, scene.tops.bottom(index) - scene.tops.top(index));
            return;
        case core::PreviewBlockKind::code:
            draw_code(scene, index, origin);
            return;
        case core::PreviewBlockKind::list_item:
            draw_marker(scene, block, origin);
            draw_text(scene, index, origin);
            return;
        case core::PreviewBlockKind::paragraph:
        case core::PreviewBlockKind::heading:
        case core::PreviewBlockKind::html:
            draw_text(scene, index, origin);
            return;
        }
    }

    // 引用の深さごとの左の線（comment・2 DIP）。前の要素が同じ深さの引用なら間も塗ってつなぐ。
    void draw_quote_rules(const PreviewScene &scene, std::size_t index)
    {
        const auto &block = scene.document.blocks.at(index);
        const auto dpi = scene.style.dpi;
        const auto offset = static_cast<float>(scene.frame.top) - scene.scroll;
        for (std::uint8_t depth = 0; depth < block.quote_depth; ++depth)
        {
            // 測った範囲の最初の要素（錨）は枠の上端から塗るので、前の要素とはつながない。
            const bool joined = index > scene.tops.first() &&
                                scene.document.blocks.at(index - 1).quote_depth > depth;
            const auto left = static_cast<float>(scene.frame.left) + preview_quote_left(depth, dpi);
            const auto top =
                offset + (joined ? scene.tops.bottom(index - 1) : scene.tops.top(index));
            target_.FillRectangle(D2D1::RectF(left, top,
                                              left + preview_pixels(quote_rule_dips, dpi),
                                              offset + scene.tops.bottom(index)),
                                  brushes_.of(PreviewInk::comment));
        }
    }

    void draw_rule(const PreviewScene &scene, float top, float extent)
    {
        const auto margin = preview_side_margin(scene.style.dpi);
        target_.FillRectangle(D2D1::RectF(static_cast<float>(scene.frame.left) + margin, top,
                                          static_cast<float>(scene.frame.right) - margin,
                                          top + extent),
                              brushes_.of(PreviewInk::comment));
    }

    // コードブロック: panel の面（角丸 5）の内側の余白 12 に折らずに描き、面の外は切る。
    void draw_code(const PreviewScene &scene, std::size_t index, D2D1_POINT_2F origin)
    {
        const auto dpi = scene.style.dpi;
        const auto extent = scene.tops.bottom(index) - scene.tops.top(index);
        const auto radius = preview_pixels(code_radius_dips, dpi);
        const auto panel = D2D1::RectF(
            origin.x, origin.y, static_cast<float>(scene.frame.right) - preview_side_margin(dpi),
            origin.y + extent);
        target_.FillRoundedRectangle(D2D1::RoundedRect(panel, radius, radius),
                                     brushes_.of(PreviewInk::panel));
        target_.PushAxisAlignedClip(panel, D2D1_ANTIALIAS_MODE_ALIASED);
        const auto padding = preview_code_padding(dpi);
        draw_text(scene, index, D2D1::Point2F(origin.x + padding, origin.y + padding));
        target_.PopAxisAlignedClip();
    }

    // 項目の記号と番号（operators）。文字の左端から 8 DIP 空けて右に揃える。記号の layout は
    // 折り返さない幅で作り、記号の列の幅は PreviewBlockTops が測る範囲の同じリストの最大の記号で
    // 決める。
    void draw_marker(const PreviewScene &scene, const core::PreviewBlock &block,
                     D2D1_POINT_2F origin)
    {
        const auto marker = preview_marker(block);
        if (marker.empty())
        {
            return;
        }
        const auto layout =
            layouts_.get(preview_marker_block(block), preview_marker_layout_width(), scene.style);
        DWRITE_TEXT_METRICS metrics{};
        if (!layout || FAILED(layout.value()->GetMetrics(&metrics)))
        {
            failure_ = RenderFailure::directwrite;
            return;
        }
        const auto left = origin.x - preview_marker_gap(scene.style.dpi) -
                          metrics.widthIncludingTrailingWhitespace;
        target_.DrawTextLayout(D2D1::Point2F(left, origin.y), layout.value().Get(),
                               brushes_.of(PreviewInk::operators), D2D1_DRAW_TEXT_OPTIONS_NONE);
    }

    void draw_text(const PreviewScene &scene, std::size_t index, D2D1_POINT_2F origin)
    {
        const auto &block = scene.document.blocks.at(index);
        const auto width =
            preview_text_width(block, static_cast<float>(core::width_of(scene.frame)),
                               scene.tops.indent(index), scene.style.dpi);
        const auto entry = layouts_.entry(block, width, scene.style);
        if (!entry)
        {
            failure_ = entry.error();
            return;
        }
        auto &held = *entry.value();
        ink_spans(block, held);
        fill_inline_code(scene, held, origin);
        target_.DrawTextLayout(origin, held.layout.Get(), brushes_.of(ink_of(block)),
                               D2D1_DRAW_TEXT_OPTIONS_NONE);
    }

    // 装飾の文字の色（行内コードは string・リンクは function・画像の代わりの文字は comment・
    // 強調と太字は周りの色のまま）。
    [[nodiscard]] static std::optional<PreviewInk> span_ink(core::PreviewStyle style) noexcept
    {
        switch (style)
        {
        case core::PreviewStyle::code:
            return PreviewInk::string;
        case core::PreviewStyle::link:
            return PreviewInk::function;
        case core::PreviewStyle::image:
            return PreviewInk::comment;
        case core::PreviewStyle::emphasis:
        case core::PreviewStyle::strong:
            return std::nullopt;
        }
        std::unreachable();
    }

    // 行の中の装飾の色（行内コードは string と panel の面・リンクは function・画像は
    // comment）。drawing effect と行内コードの範囲の hit test は保持の entry に 1 回だけ置き、
    // entry の印（効果をかけたときの foreground の brush）が今の brush の組と違うときだけ
    // かけ直す（テーマか描画先が変わって brush が作り直されたとき・ADR 0155 の追補・#876）。
    // 印は brush を参照で持つので、同じ番地に別の brush が作られて取り違えることはない。
    void ink_spans(const core::PreviewBlock &block, PreviewTextLayout &held)
    {
        auto *const stamp = brushes_.of(PreviewInk::foreground);
        if (held.inked.has_value() && held.inked.value().Get() == stamp)
        {
            return;
        }
        held.inked.reset();
        held.code_runs.clear();
        const auto shown = preview_shown_text(block);
        std::vector<DWRITE_HIT_TEST_METRICS> code_runs;
        for (const auto &span : block.spans)
        {
            if (!ink_span(held, shown, span, code_runs))
            {
                failure_ = RenderFailure::directwrite;
                return;
            }
        }
        held.code_runs = std::move(code_runs);
        held.inked = Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>(stamp);
    }

    // 1 つの装飾の効果をかけ、行内コードなら範囲の hit test を code_runs に足す。
    [[nodiscard]] bool ink_span(PreviewTextLayout &held, std::string_view shown,
                                const core::PreviewSpan &span,
                                std::vector<DWRITE_HIT_TEST_METRICS> &code_runs)
    {
        const auto begin = preview_units(shown, span.begin.value);
        const auto end = preview_units(shown, span.end.value);
        const DWRITE_TEXT_RANGE range{begin, end > begin ? end - begin : 0};
        if (span.style == core::PreviewStyle::code)
        {
            const auto runs = runs_of(*held.layout.Get(), range);
            code_runs.insert(code_runs.end(), runs.begin(), runs.end());
        }
        const auto colour = span_ink(span.style);
        return !failure_.has_value() &&
               (!colour.has_value() ||
                SUCCEEDED(held.layout->SetDrawingEffect(brushes_.of(colour.value()), range)));
    }

    // 行内コードの面（panel・角丸 3・左右 4 DIP）。折り返した範囲は画面行ごとの矩形で、
    // entry の hit test（原点 0,0）を描く原点へずらす。
    void fill_inline_code(const PreviewScene &scene, const PreviewTextLayout &held,
                          D2D1_POINT_2F origin)
    {
        const auto dpi = scene.style.dpi;
        const auto padding = preview_pixels(inline_padding_dips, dpi);
        const auto radius = preview_pixels(inline_radius_dips, dpi);
        for (const auto &run : held.code_runs)
        {
            const auto left = run.left + origin.x;
            const auto top = run.top + origin.y;
            const auto inset = run.height * inline_inset_ratio;
            const auto rect = D2D1::RectF(left - padding, top + inset, left + run.width + padding,
                                          top + run.height - inset);
            target_.FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius),
                                         brushes_.of(PreviewInk::panel));
        }
    }

    [[nodiscard]] std::vector<DWRITE_HIT_TEST_METRICS> runs_of(IDWriteTextLayout &layout,
                                                               DWRITE_TEXT_RANGE range)
    {
        if (range.length == 0)
        {
            return {};
        }
        UINT32 count = 0;
        const auto counted = layout.HitTestTextRange(range.startPosition, range.length, 0.0F, 0.0F,
                                                     nullptr, 0, &count);
        std::vector<DWRITE_HIT_TEST_METRICS> runs(count);
        if ((FAILED(counted) && counted != E_NOT_SUFFICIENT_BUFFER) ||
            FAILED(layout.HitTestTextRange(range.startPosition, range.length, 0.0F, 0.0F,
                                           runs.data(), count, &count)))
        {
            failure_ = RenderFailure::directwrite;
            return {};
        }
        runs.resize(count);
        return runs;
    }

    Target &target_;
    PreviewTextLayouts &layouts_;
    const PreviewBrushes &brushes_;
    std::optional<RenderFailure> &failure_;
};
} // namespace nenenib::ui::win32
