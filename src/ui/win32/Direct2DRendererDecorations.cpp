// 本文の飾りの層（ADR 0193 の決定 1・2・6・#1975・#1977）。application が行に置いた桁の欄を、保持
// した layout の外に矩形と線で重ねる。段 1 はインデントガイド（下の飾り）、段 2
// は不可視文字の印（上の 飾り）で、段 3（括弧）は draw_decorations_under へ足す。オフの道は frame
// の初めの真偽 1 つと、行ごと の欄の有無の読み 1 つだけ。
#include "Direct2DRenderer.hpp"

#include "Direct2DRendererDetail.hpp"
#include "IndentGuidePainter.hpp"
#include "IndentGuideRow.hpp"
#include "InvisibleBox.hpp"
#include "InvisibleKind.hpp"
#include "InvisibleMark.hpp"
#include "InvisibleMarkBoxes.hpp"
#include "InvisibleMarkPainter.hpp"
#include "InvisibleMarkUnits.hpp"
#include "Invisibles.hpp"
#include "LineBreakPlace.hpp"
#include "TextLayoutPoint.hpp"

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
// layout の座標の升を描く先の座標へ（行の原点を足す）。
void moved(std::vector<InvisibleBox> &boxes, const core::LayoutRect &area) noexcept
{
    for (auto &placed : boxes)
    {
        placed.box.left += static_cast<float>(area.left);
        placed.box.right += static_cast<float>(area.left);
        placed.box.top += static_cast<float>(area.top);
        placed.box.bottom += static_cast<float>(area.top);
    }
}
} // namespace

void Direct2DRenderer::begin_decorations(const application::EditorFrame &frame)
{
    guide_paint_.begin(frame.settings, frame.body, frame.palette);
    decoration_palette_ = frame.palette;
}

void Direct2DRenderer::draw_decorations_under(const BodyPane &pane, const core::LayoutRect &area,
                                              const application::LineView &line)
{
    draw_indent_guides(pane, area, line);
}

void Direct2DRenderer::draw_decorations_over(const BodyPane &pane, IDWriteTextLayout *text,
                                             const core::LayoutRect &area,
                                             const application::LineView &line)
{
    draw_invisibles(pane, text, area, line);
}

void Direct2DRenderer::draw_indent_guides(const BodyPane &pane, const core::LayoutRect &area,
                                          const application::LineView &line)
{
    const core::IndentGuidePaint *paint = guide_paint_.shown();
    if (!line.indent_guides.has_value() || paint == nullptr)
    {
        return;
    }
    const auto space = body_text_.space_width(core::DisplayForm::text);
    if (!space.has_value())
    {
        return;
    }
    IndentGuidePainter::paint(*context_.Get(), brush_.Get(),
                              indent_guide_row(pane.body, area, line.indent_guides.value(),
                                               IndentGuideScale{space.value(), dpi_}),
                              *paint);
}

std::vector<InvisibleBox>
Direct2DRenderer::invisible_boxes_of(IDWriteTextLayout *text, const core::LayoutRect &area,
                                     const std::vector<InvisibleMark> &marks)
{
    auto painter = body_painter();
    const auto *entry = painter.glyph_entry(text, area);
    const auto stop = body_text_.tab_stop_width(core::DisplayForm::text);
    DWRITE_LINE_SPACING_METHOD method{};
    float spacing = 0;
    float baseline = 0;
    std::vector<InvisibleMark> unresolved;
    std::optional<std::vector<InvisibleBox>> boxes;
    if (entry != nullptr && entry->glyphs_ready && stop.has_value() &&
        SUCCEEDED(text->GetLineSpacing(&method, &spacing, &baseline)) &&
        method == DWRITE_LINE_SPACING_METHOD_UNIFORM)
    {
        boxes = invisible_boxes(entry->glyphs, marks, InvisibleScale{spacing, stop.value()},
                                unresolved);
    }
    if (boxes.has_value())
    {
        moved(boxes.value(), area);
    }
    // 保持できない行（glyphs_ready
    // でない・位置を引けない）は字ごと、保持できる行は左の字が保持に無い Tab だけを
    // HitTestTextRange で引く（決定 10・#2047 のレビュー R2）。
    const bool kept = boxes.has_value();
    std::vector<InvisibleBox> placed = std::move(boxes).value_or(std::vector<InvisibleBox>{});
    for (const InvisibleMark &mark : kept ? unresolved : marks)
    {
        for (const auto &hit : painter.runs_of(text, area, DWRITE_TEXT_RANGE{mark.unit, 1}))
        {
            placed.push_back(InvisibleBox{
                D2D1::RectF(hit.left, hit.top, hit.left + hit.width, hit.top + hit.height),
                mark.kind});
        }
    }
    return placed;
}

void Direct2DRenderer::draw_invisibles(const BodyPane &pane, IDWriteTextLayout *text,
                                       const core::LayoutRect &area,
                                       const application::LineView &line)
{
    if (!line.invisibles.has_value())
    {
        return;
    }
    const auto height = static_cast<float>(body_line_height_);
    const auto left = static_cast<float>(pane.body.content.left);
    const auto right = static_cast<float>(pane.body.content.right);
    for (const InvisibleBox &placed : invisible_boxes_of(text, area, invisible_marks_of(line)))
    {
        // 本文の欄の左右の外の印は描かない（#2047 のレビュー R1）。
        if (placed.box.right < left || placed.box.left > right)
        {
            continue;
        }
        brush_->SetColor(
            detail::to_color(core::invisible_mark_color(decoration_palette_, placed.kind)));
        InvisibleMarkPainter::paint(*context_.Get(), brush_.Get(), placed, height);
    }
    draw_line_break(pane, text, area, line);
}

void Direct2DRenderer::draw_line_break(const BodyPane &pane, IDWriteTextLayout *text,
                                       const core::LayoutRect &area,
                                       const application::LineView &line)
{
    const auto mark =
        line.invisibles.has_value() ? line.invisibles.value().line_break : std::nullopt;
    const auto space = body_text_.space_width(core::DisplayForm::text);
    if (!mark.has_value() || !space.has_value())
    {
        return;
    }
    // 行の終わりの x（折り返した行は最後の画面行）を 1 回求める（決定 10）。
    const auto point = text_layout_point(text, line_end_unit(line), core::CaretAffinity::trailing);
    if (!point)
    {
        text_failure_ = RenderFailure::directwrite;
        return;
    }
    LineBreakPlace place{D2D1::Point2F(static_cast<float>(area.left) + point.value().x,
                                       static_cast<float>(area.top) + point.value().y),
                         mark.value(), static_cast<float>(body_line_height_), space.value()};
    const auto shown = line_break_left(place.at.x, InvisibleMarkPainter::line_break_width(place),
                                       static_cast<float>(pane.body.content.right));
    if (!shown.has_value())
    {
        return;
    }
    place.at.x = shown.value();
    brush_->SetColor(detail::to_color(core::line_break_mark_color(decoration_palette_)));
    InvisibleMarkPainter::paint_line_break(*context_.Get(), brush_.Get(), place);
}
} // namespace nenenib::ui::win32
