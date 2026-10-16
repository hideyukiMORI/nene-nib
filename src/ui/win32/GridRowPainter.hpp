#pragma once

#include "BodyTextLayout.hpp"
#include "GridRowCanvas.hpp"
#include "GridRowView.hpp"
#include "RenderFailure.hpp"
#include "RgbaColor.hpp"
#include "TextLayoutRequest.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <optional>
#include <utility>
#include <windows.h>

namespace nenenib::ui::win32
{
// 格子の 1 画面行を描く（ADR 0140 の決定 5）。文字組みは画面行の文字列 1 つを等幅の書式で作り
// （BodyTextLayouts が字形を升の左端へ揃える）、選択・検索の当たり・変換の文節・キャレットは
// application が写した升の範囲の矩形から塗る。DirectWrite の位置解決（HitTest）は使わない。
// Target は ID2D1RenderTarget の同じ名前のメソッドを持つ描画先、Layouts は BodyTextLayouts の
// get / glyphs_of を持つ文字組み（試験は替え玉で数える・#531 の形）。brush が空なら色を変えない。
template <typename Target, typename Layouts> class GridRowPainter final
{
  public:
    GridRowPainter(Target &target, ID2D1SolidColorBrush *brush, Layouts &layouts,
                   std::optional<RenderFailure> &failure)
        : target_(target), brush_(brush), layouts_(layouts), failure_(failure)
    {
    }

    // 描いたキャレットの矩形（IME の候補窓の位置）。キャレットの画面行でなければ空。
    std::optional<RECT> draw(const application::GridRowView &row, const GridRowCanvas &canvas)
    {
        const auto made = layouts_.get(application::TextLayoutRequest{
            row.text, canvas.settings, canvas.geometry, core::DisplayForm::grid, row.starts});
        if (!made)
        {
            failure_ = RenderFailure::directwrite;
            return std::nullopt;
        }
        auto *layout = made.value().Get();
        draw_marks(row, canvas);
        color(canvas.palette.text);
        draw_text(layout, canvas, std::nullopt);
        color(canvas.palette.muted);
        for (const auto &span : row.replaced)
        {
            draw_text(layout, canvas, rectangle(canvas, span));
        }
        draw_clauses(layout, row, canvas);
        if (row.current_match.has_value())
        {
            outline(canvas, row.current_match.value());
        }
        const auto &caret = row.caret;
        if (!caret.has_value() || !canvas.caret_shown)
        {
            return std::nullopt;
        }
        return draw_caret(layout, caret.value(), row.composed_at.has_value(), canvas);
    }

    // 升の範囲の矩形（物理画素）。行の上端から行の高さ。
    [[nodiscard]] static D2D1_RECT_F rectangle(const GridRowCanvas &canvas,
                                               application::GridCellSpan span) noexcept
    {
        const auto left = static_cast<float>(canvas.area.left);
        return D2D1::RectF(left + (static_cast<float>(span.begin) * canvas.cell),
                           static_cast<float>(canvas.area.top),
                           left + (static_cast<float>(span.end) * canvas.cell),
                           static_cast<float>(canvas.area.bottom));
    }

  private:
    void color(core::RgbaColor value)
    {
        if (brush_ != nullptr)
        {
            brush_->SetColor(D2D1::ColorF(static_cast<float>(value.color.red) / full_channel,
                                          static_cast<float>(value.color.green) / full_channel,
                                          static_cast<float>(value.color.blue) / full_channel,
                                          static_cast<float>(value.alpha) / full_channel));
        }
    }

    void color(core::RgbColor value)
    {
        color(core::RgbaColor{value, opaque});
    }

    // 検索の当たり → 選択（ADR 0037 の決定 5 と同じ順）。改行を越える選択は行末の後ろに印。
    void draw_marks(const application::GridRowView &row, const GridRowCanvas &canvas)
    {
        color(canvas.palette.search);
        for (const auto &span : row.matches)
        {
            target_.FillRectangle(rectangle(canvas, span), brush_);
        }
        color(canvas.palette.selection);
        if (row.selection.has_value())
        {
            target_.FillRectangle(rectangle(canvas, row.selection.value()), brush_);
        }
        if (row.newline_selected)
        {
            auto mark =
                rectangle(canvas, application::GridCellSpan{row.starts.back(), row.starts.back()});
            mark.right = mark.left + static_cast<float>(canvas.newline_mark);
            target_.FillRectangle(mark, brush_);
        }
    }

    // 保持した字形で描く（ADR 0073）。保持できなければ layout をそのまま描く。clip があれば
    // その中だけ。
    void draw_text(IDWriteTextLayout *layout, const GridRowCanvas &canvas,
                   std::optional<D2D1_RECT_F> clip)
    {
        const auto origin = D2D1::Point2F(static_cast<float>(canvas.area.left),
                                          static_cast<float>(canvas.area.top));
        target_.PushAxisAlignedClip(
            clip.value_or(D2D1::RectF(origin.x, origin.y, static_cast<float>(canvas.area.right),
                                      static_cast<float>(canvas.area.bottom))),
            D2D1_ANTIALIAS_MODE_ALIASED);
        const auto *entry = layouts_.glyphs_of(layout, 0);
        if (entry == nullptr || !entry->glyphs_ready)
        {
            target_.DrawTextLayout(origin, layout, brush_, D2D1_DRAW_TEXT_OPTIONS_NONE);
            target_.PopAxisAlignedClip();
            return;
        }
        for (const auto &stored : entry->glyphs)
        {
            const DWRITE_GLYPH_RUN run{stored.face.Get(),
                                       stored.em,
                                       static_cast<UINT32>(stored.indices.size()),
                                       stored.indices.data(),
                                       stored.advances.data(),
                                       stored.offsets.empty() ? nullptr : stored.offsets.data(),
                                       stored.sideways,
                                       stored.bidi};
            target_.DrawGlyphRun(
                D2D1::Point2F(origin.x + stored.origin.x, origin.y + stored.origin.y), &run, brush_,
                stored.measuring);
        }
        target_.PopAxisAlignedClip();
    }

    void outline(const GridRowCanvas &canvas, application::GridCellSpan span)
    {
        color(canvas.palette.accent);
        const float inset = canvas.stroke / 2.0F;
        auto box = rectangle(canvas, span);
        box = D2D1::RectF(box.left + inset, box.top + inset, box.right - inset, box.bottom - inset);
        target_.DrawRectangle(box, brush_, canvas.stroke);
    }

    void underline(const GridRowCanvas &canvas, D2D1_RECT_F box, std::int32_t thickness)
    {
        const auto bottom = box.bottom - static_cast<float>(canvas.caret_inset);
        target_.FillRectangle(
            D2D1::RectF(box.left, bottom - static_cast<float>(thickness), box.right, bottom),
            brush_);
    }

    // 文節の範囲（変換中の文字列の中のバイト）を、差し込んだ所からの code point → 升へ。
    [[nodiscard]] static application::GridCellSpan
    clause_cells(const application::GridRowView &row, const application::CompositionView &shown,
                 core::OffsetRange range)
    {
        const std::string_view utf8 = shown.utf8;
        const auto at = row.composed_at.value_or(0);
        const auto from = at + core::code_point_count(utf8.substr(0, range.begin.value));
        const auto stop = at + core::code_point_count(utf8.substr(0, range.end.value));
        const auto last = row.starts.size() - 1;
        return application::GridCellSpan{row.starts.at(std::min(from, last)),
                                         row.starts.at(std::min(stop, last))};
    }

    void draw_clauses(IDWriteTextLayout *layout, const application::GridRowView &row,
                      const GridRowCanvas &canvas)
    {
        if (!row.composed_at.has_value() || !canvas.composition.has_value())
        {
            return;
        }
        const auto &shown = canvas.composition.value();
        for (const auto &clause : shown.underlines)
        {
            const auto box = rectangle(canvas, clause_cells(row, shown, clause.range));
            switch (clause.emphasis)
            {
            case core::ClauseEmphasis::target:
                color(canvas.palette.selection);
                target_.FillRectangle(box, brush_);
                color(canvas.palette.text);
                draw_text(layout, canvas, box);
                color(canvas.palette.accent);
                underline(canvas, box, canvas.target_underline);
                break;
            case core::ClauseEmphasis::other:
                color(canvas.palette.ime);
                draw_text(layout, canvas, box);
                underline(canvas, box, canvas.other_underline);
                break;
            }
        }
    }

    // 縦棒は升の左端、ブロックは升の範囲を塗って字を on_accent で描き直す。変換中は縦棒。
    RECT draw_caret(IDWriteTextLayout *layout, application::GridCellSpan caret, bool composing,
                    const GridRowCanvas &canvas)
    {
        auto box = rectangle(canvas, caret);
        color(canvas.palette.accent);
        if (composing || canvas.shape == core::CaretShape::bar)
        {
            box = D2D1::RectF(box.left, box.top + static_cast<float>(canvas.caret_inset),
                              box.left + static_cast<float>(canvas.caret_width),
                              box.bottom - static_cast<float>(canvas.caret_inset));
            target_.FillRectangle(box, brush_);
            return rect_of(box);
        }
        target_.FillRoundedRectangle(
            D2D1::RoundedRect(box, canvas.block_radius, canvas.block_radius), brush_);
        color(canvas.palette.on_accent);
        draw_text(layout, canvas, box);
        return rect_of(box);
    }

    [[nodiscard]] static RECT rect_of(D2D1_RECT_F box) noexcept
    {
        return RECT{static_cast<LONG>(box.left), static_cast<LONG>(box.top),
                    static_cast<LONG>(box.right), static_cast<LONG>(box.bottom)};
    }

    static constexpr float full_channel = 255.0F;
    static constexpr std::uint8_t opaque = 255;

    Target &target_;
    ID2D1SolidColorBrush *brush_;
    Layouts &layouts_;
    std::optional<RenderFailure> &failure_;
};
} // namespace nenenib::ui::win32
