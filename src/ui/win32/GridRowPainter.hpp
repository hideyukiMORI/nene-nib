#pragma once

#include "BodyColors.hpp"
#include "BodyTextPainter.hpp"
#include "CaretShape.hpp"
#include "ColoredRun.hpp"
#include "GridRowCanvas.hpp"
#include "GridRowView.hpp"
#include "PaneRole.hpp"
#include "RenderFailure.hpp"
#include "RgbaColor.hpp"
#include "SyntaxColor.hpp"
#include "TextLayoutRequest.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>
#include <windows.h>

namespace nenenib::ui::win32
{
// 格子の 1 画面行を描く（ADR 0140 の決定 5）。文字組みは画面行の文字列 1 つを等幅の書式で作り
// （BodyTextLayouts が字形を升の左端へ揃える）、選択・検索の当たり・変換の文節・キャレットは
// application が写した升の範囲の矩形から塗る。DirectWrite の位置解決（HitTest）は使わない。
// 字形は本文と同じ BodyTextPainter で描き、格子は字形のループを持たない（#569）。
// Target は ID2D1RenderTarget の同じ名前のメソッドを持つ描画先、Layouts は BodyTextLayouts の
// get / glyphs_of を持つ文字組み（試験は替え玉で数える・#531 の形）。brush が空なら色を変えない。
template <typename Target, typename Layouts> class GridRowPainter final
{
  public:
    GridRowPainter(Target &target, ID2D1SolidColorBrush *brush, Layouts &layouts,
                   std::optional<RenderFailure> &failure)
        : target_(target), brush_(brush), layouts_(layouts), failure_(failure),
          body_(target, brush, layouts, failure)
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
        draw_row_text(layout, row, canvas);
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
        switch (canvas.role)
        {
        case PaneRole::active:
            return draw_caret(layout, caret.value(), row.composed_at.has_value(), canvas);
        case PaneRole::passive:
            draw_passive_caret(caret.value(), canvas);
            return std::nullopt;
        }
        std::unreachable();
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

    // 構文の色の升の範囲を画面行の文字列の UTF-16 の範囲へ写す（#839）。升の境の code point は、
    // その升で始まり升を持つ最初の code point（幅 0 の結合文字は前の文字の側・ADR 0053）。
    [[nodiscard]] static std::vector<ColoredRun> colored_runs(const application::GridRowView &row,
                                                              const core::SyntaxPalette &body)
    {
        std::vector<UINT32> units{0};
        for (std::size_t byte = 0; byte < row.text.size();)
        {
            const auto value = core::code_point_at(row.text, core::Offset{byte});
            units.push_back(units.back() + (value > utf16_single ? 2U : 1U));
            byte = core::next_code_point(row.text, core::Offset{byte}).value;
        }
        std::vector<ColoredRun> colored;
        colored.reserve(row.colors.size());
        for (const auto &span : row.colors)
        {
            const auto from =
                units.at(std::min(code_point_of(row.starts, span.cells.begin), units.size() - 1));
            const auto stop =
                units.at(std::min(code_point_of(row.starts, span.cells.end), units.size() - 1));
            if (stop > from)
            {
                colored.push_back(ColoredRun{
                    DWRITE_TEXT_RANGE{from, stop - from},
                    to_color(core::RgbaColor{core::syntax_color(body, span.role), opaque})});
            }
        }
        return colored;
    }

  private:
    [[nodiscard]] static D2D1_COLOR_F to_color(core::RgbaColor value) noexcept
    {
        return D2D1::ColorF(static_cast<float>(value.color.red) / full_channel,
                            static_cast<float>(value.color.green) / full_channel,
                            static_cast<float>(value.color.blue) / full_channel,
                            static_cast<float>(value.alpha) / full_channel);
    }

    void color(D2D1_COLOR_F value)
    {
        if (brush_ != nullptr)
        {
            brush_->SetColor(value);
        }
    }

    void color(core::RgbaColor value)
    {
        color(to_color(value));
    }

    void color(core::RgbColor value)
    {
        color(core::RgbaColor{value, opaque});
    }

    // 升 cell で始まり升を持つ最初の code point の添字（starts の末尾は升の和）。
    [[nodiscard]] static std::size_t code_point_of(const std::vector<std::size_t> &starts,
                                                   std::size_t cell)
    {
        const auto last = starts.size() - 1;
        const std::span<const std::size_t> heads(starts.data(), last);
        auto index = static_cast<std::size_t>(
            std::distance(heads.begin(), std::ranges::lower_bound(heads, cell)));
        while (index < last && starts.at(index) == cell && starts.at(index + 1) == cell)
        {
            ++index;
        }
        return index;
    }

    // 画面行の字（#839）。色の区間が無ければ text の色で 1 回、あれば本文と同じ色つきの道
    // （BodyTextPainter::draw_colored_body_text）で区間ごとの構文の色と区間の外の text の色。
    void draw_row_text(IDWriteTextLayout *layout, const application::GridRowView &row,
                       const GridRowCanvas &canvas)
    {
        if (row.colors.empty())
        {
            color(canvas.palette.text);
            draw_text(layout, canvas, std::nullopt);
            return;
        }
        const auto colored = colored_runs(row, canvas.body);
        body_.draw_colored_body_text(
            layout, canvas.area,
            BodyColors{colored, to_color(core::RgbaColor{canvas.palette.text, opaque})},
            [this](const D2D1_COLOR_F &value) { color(value); });
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

    // clip（無ければ行の area）の中に、本文と同じ経路で行の layout を描く（ADR 0073）。
    // 格子の layout の幅は行の area の幅と同じなので、保持の左の鍵は 0 で本文と同じ（#569）。
    void draw_text(IDWriteTextLayout *layout, const GridRowCanvas &canvas,
                   std::optional<D2D1_RECT_F> clip)
    {
        target_.PushAxisAlignedClip(
            clip.value_or(D2D1::RectF(
                static_cast<float>(canvas.area.left), static_cast<float>(canvas.area.top),
                static_cast<float>(canvas.area.right), static_cast<float>(canvas.area.bottom))),
            D2D1_ANTIALIAS_MODE_ALIASED);
        body_.draw_body_text(layout, canvas.area, D2D1_DRAW_TEXT_OPTIONS_NONE);
        target_.PopAxisAlignedClip();
    }

    void outline(const GridRowCanvas &canvas, application::GridCellSpan span)
    {
        color(canvas.palette.accent);
        frame(canvas, span);
    }

    // 升の範囲を stroke の枠線で囲む。色は呼び手が置く。
    void frame(const GridRowCanvas &canvas, application::GridCellSpan span)
    {
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

    // もう 1 つの枠の印（ADR 0148 の決定 8・採用案の案 A）。muted の縦棒か、升の範囲の中空の枠。
    // 字は描き直さない。IME の候補窓の位置にはしない（呼び手へ返さない）。
    void draw_passive_caret(application::GridCellSpan caret, const GridRowCanvas &canvas)
    {
        color(canvas.palette.muted);
        switch (canvas.shape)
        {
        case core::CaretShape::bar:
        {
            const auto box = rectangle(canvas, caret);
            target_.FillRectangle(D2D1::RectF(box.left,
                                              box.top + static_cast<float>(canvas.caret_inset),
                                              box.left + static_cast<float>(canvas.caret_width),
                                              box.bottom - static_cast<float>(canvas.caret_inset)),
                                  brush_);
            return;
        }
        case core::CaretShape::block:
            frame(canvas, caret);
            return;
        }
        std::unreachable();
    }

    [[nodiscard]] static RECT rect_of(D2D1_RECT_F box) noexcept
    {
        return RECT{static_cast<LONG>(box.left), static_cast<LONG>(box.top),
                    static_cast<LONG>(box.right), static_cast<LONG>(box.bottom)};
    }

    static constexpr float full_channel = 255.0F;
    static constexpr std::uint8_t opaque = 255;
    // UTF-16 の 1 単位に収まる最大の code point。
    static constexpr char32_t utf16_single = 0xFFFF;

    Target &target_;
    ID2D1SolidColorBrush *brush_;
    Layouts &layouts_;
    std::optional<RenderFailure> &failure_;
    BodyTextPainter<Target, Layouts> body_;
};
} // namespace nenenib::ui::win32
