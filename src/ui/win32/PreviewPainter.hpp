#pragma once

#include "LayoutRect.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewInk.hpp"
#include "PreviewPlacement.hpp"
#include "PreviewScene.hpp"
#include "PreviewTextLayouts.hpp"
#include "RenderFailure.hpp"

#include <algorithm>
#include <cstddef>
#include <d2d1.h>
#include <dwrite.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
// Markdown プレビューを描く経路（ADR 0151 の決定 17・採用案 D62 の案 B）。上から高さを足した
// 部分和（PreviewBlockTops）で見えている要素だけを DrawTextLayout で描き、面と線は
// FillRectangle / FillRoundedRectangle。Target は製品では Direct2D の描画先
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
    static constexpr float marker_gap_dips = 8.0F;
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
        const auto dpi = scene.style.dpi;
        const auto origin = D2D1::Point2F(
            static_cast<float>(scene.frame.left) + preview_indent(block, dpi),
            static_cast<float>(scene.frame.top) + scene.tops.top(index) - scene.scroll);
        draw_quote_rules(scene, index);
        switch (block.kind)
        {
        case core::PreviewBlockKind::rule:
            draw_rule(scene, origin.y, scene.tops.bottom(index) - scene.tops.top(index));
            return;
        case core::PreviewBlockKind::code:
            draw_code(scene, block, origin, scene.tops.bottom(index) - scene.tops.top(index));
            return;
        case core::PreviewBlockKind::list_item:
            draw_marker(scene, block, origin);
            draw_text(scene, block, origin);
            return;
        case core::PreviewBlockKind::paragraph:
        case core::PreviewBlockKind::heading:
        case core::PreviewBlockKind::html:
            draw_text(scene, block, origin);
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
            const bool joined =
                index > 0 && scene.document.blocks.at(index - 1).quote_depth > depth;
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
    void draw_code(const PreviewScene &scene, const core::PreviewBlock &block, D2D1_POINT_2F origin,
                   float extent)
    {
        const auto dpi = scene.style.dpi;
        const auto radius = preview_pixels(code_radius_dips, dpi);
        const auto panel = D2D1::RectF(
            origin.x, origin.y, static_cast<float>(scene.frame.right) - preview_side_margin(dpi),
            origin.y + extent);
        target_.FillRoundedRectangle(D2D1::RoundedRect(panel, radius, radius),
                                     brushes_.of(PreviewInk::panel));
        target_.PushAxisAlignedClip(panel, D2D1_ANTIALIAS_MODE_ALIASED);
        const auto padding = preview_code_padding(dpi);
        draw_text(scene, block, D2D1::Point2F(origin.x + padding, origin.y + padding));
        target_.PopAxisAlignedClip();
    }

    // 項目の記号と番号（operators）。文字の左端から 8 DIP 空けて右に揃える。
    void draw_marker(const PreviewScene &scene, const core::PreviewBlock &block,
                     D2D1_POINT_2F origin)
    {
        const auto marker = preview_marker(block);
        if (marker.empty())
        {
            return;
        }
        const auto dpi = scene.style.dpi;
        const core::PreviewBlock shown{core::PreviewBlockKind::paragraph,
                                       0,
                                       0,
                                       0,
                                       {},
                                       block.source_begin,
                                       std::string(marker),
                                       {}};
        const auto layout = layouts_.get(shown, preview_list_indent(dpi), scene.style);
        DWRITE_TEXT_METRICS metrics{};
        if (!layout || FAILED(layout.value()->GetMetrics(&metrics)))
        {
            failure_ = RenderFailure::directwrite;
            return;
        }
        const auto left = origin.x - preview_pixels(marker_gap_dips, dpi) -
                          metrics.widthIncludingTrailingWhitespace;
        target_.DrawTextLayout(D2D1::Point2F(left, origin.y), layout.value().Get(),
                               brushes_.of(PreviewInk::operators), D2D1_DRAW_TEXT_OPTIONS_NONE);
    }

    void draw_text(const PreviewScene &scene, const core::PreviewBlock &block, D2D1_POINT_2F origin)
    {
        const auto width = preview_text_width(
            block, static_cast<float>(core::width_of(scene.frame)), scene.style.dpi);
        const auto layout = layouts_.get(block, width, scene.style);
        if (!layout)
        {
            failure_ = layout.error();
            return;
        }
        paint_spans(scene, block, *layout.value().Get(), origin);
        target_.DrawTextLayout(origin, layout.value().Get(), brushes_.of(ink_of(block)),
                               D2D1_DRAW_TEXT_OPTIONS_NONE);
    }

    // 行の中の装飾の色（行内コードは string と panel の面・リンクは function）。layout の
    // drawing effect は描くたびに置き直す（保持の鍵にテーマを入れない）。
    void paint_spans(const PreviewScene &scene, const core::PreviewBlock &block,
                     IDWriteTextLayout &layout, D2D1_POINT_2F origin)
    {
        const auto shown = preview_shown_text(block);
        for (const auto &span : block.spans)
        {
            const auto begin = preview_units(shown, span.begin.value);
            const auto end = preview_units(shown, span.end.value);
            const DWRITE_TEXT_RANGE range{begin, end > begin ? end - begin : 0};
            if (span.style == core::PreviewStyle::code)
            {
                fill_inline_code(scene, layout, range, origin);
                static_cast<void>(layout.SetDrawingEffect(brushes_.of(PreviewInk::string), range));
            }
            if (span.style == core::PreviewStyle::link)
            {
                static_cast<void>(
                    layout.SetDrawingEffect(brushes_.of(PreviewInk::function), range));
            }
        }
    }

    // 行内コードの面（panel・角丸 3・左右 4 DIP）。折り返した範囲は画面行ごとの矩形。
    void fill_inline_code(const PreviewScene &scene, IDWriteTextLayout &layout,
                          DWRITE_TEXT_RANGE range, D2D1_POINT_2F origin)
    {
        const auto dpi = scene.style.dpi;
        const auto padding = preview_pixels(inline_padding_dips, dpi);
        const auto radius = preview_pixels(inline_radius_dips, dpi);
        for (const auto &run : runs_of(layout, range, origin))
        {
            const auto inset = run.height * inline_inset_ratio;
            const auto rect =
                D2D1::RectF(run.left - padding, run.top + inset, run.left + run.width + padding,
                            run.top + run.height - inset);
            target_.FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius),
                                         brushes_.of(PreviewInk::panel));
        }
    }

    [[nodiscard]] std::vector<DWRITE_HIT_TEST_METRICS>
    runs_of(IDWriteTextLayout &layout, DWRITE_TEXT_RANGE range, D2D1_POINT_2F origin)
    {
        if (range.length == 0)
        {
            return {};
        }
        UINT32 count = 0;
        const auto counted = layout.HitTestTextRange(range.startPosition, range.length, origin.x,
                                                     origin.y, nullptr, 0, &count);
        std::vector<DWRITE_HIT_TEST_METRICS> runs(count);
        if ((FAILED(counted) && counted != E_NOT_SUFFICIENT_BUFFER) ||
            FAILED(layout.HitTestTextRange(range.startPosition, range.length, origin.x, origin.y,
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
