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
#include "Invisibles.hpp"
#include "LineBreakPlace.hpp"
#include "TextLayoutPoint.hpp"
#include "Utf8.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
// Tab の止まりの空白の数（BodyTextLayouts::set_spacing の SetIncrementalTabStop と同じ 8）。
constexpr float tab_stop_spaces = 8.0F;

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
}

void Direct2DRenderer::draw_decorations_under(const BodyPane &pane, const core::LayoutRect &area,
                                              const application::LineView &line)
{
    draw_indent_guides(pane, area, line);
}

void Direct2DRenderer::draw_decorations_over(const application::EditorFrame &frame,
                                             IDWriteTextLayout *text, const core::LayoutRect &area,
                                             const application::LineView &line)
{
    draw_invisibles(frame, text, area, line);
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
    const auto space = body_text_.space_width(core::DisplayForm::text);
    DWRITE_LINE_SPACING_METHOD method{};
    float spacing = 0;
    float baseline = 0;
    if (entry != nullptr && entry->glyphs_ready && space.has_value() &&
        SUCCEEDED(text->GetLineSpacing(&method, &spacing, &baseline)) &&
        method == DWRITE_LINE_SPACING_METHOD_UNIFORM)
    {
        auto boxes =
            invisible_boxes(entry->glyphs, marks, spacing, space.value() * tab_stop_spaces);
        if (boxes.has_value())
        {
            moved(boxes.value(), area);
            return std::move(boxes).value();
        }
    }
    // 保持できない行（glyphs_ready でない・位置を引けない）だけ字ごとの HitTestTextRange（決定
    // 10）。
    std::vector<InvisibleBox> boxes;
    for (const InvisibleMark &mark : marks)
    {
        for (const auto &hit : painter.runs_of(text, area, DWRITE_TEXT_RANGE{mark.unit, 1}))
        {
            boxes.push_back(InvisibleBox{
                D2D1::RectF(hit.left, hit.top, hit.left + hit.width, hit.top + hit.height),
                mark.kind});
        }
    }
    return boxes;
}

void Direct2DRenderer::draw_invisibles(const application::EditorFrame &frame,
                                       IDWriteTextLayout *text, const core::LayoutRect &area,
                                       const application::LineView &line)
{
    if (!line.invisibles.has_value())
    {
        return;
    }
    const application::LineInvisibles &shown = line.invisibles.value();
    LineUtf16Evaluation positions(line.display.text);
    std::vector<InvisibleMark> marks;
    for (const core::InvisibleSpan &span : shown.spans)
    {
        const DWRITE_TEXT_RANGE range = range_of(
            line, core::SelectionSpan{core::SelectionPresence::present, span.begin, span.end},
            positions);
        for (UINT32 unit = range.startPosition; unit < range.startPosition + range.length; ++unit)
        {
            marks.push_back(InvisibleMark{unit, span.kind});
        }
    }
    const auto height = static_cast<float>(body_line_height_);
    for (const InvisibleBox &placed : invisible_boxes_of(text, area, marks))
    {
        brush_->SetColor(detail::to_color(core::invisible_mark_color(frame.palette, placed.kind)));
        InvisibleMarkPainter::paint(*context_.Get(), brush_.Get(), placed, height);
    }
    const auto space = body_text_.space_width(core::DisplayForm::text);
    if (!shown.line_break.has_value() || !space.has_value())
    {
        return;
    }
    // 行の終わりの x（折り返した行は最後の画面行）を 1 回求める（決定 10）。
    const core::Column end{core::code_point_count(line.text) + 1};
    const auto stop =
        range_of(line, core::SelectionSpan{core::SelectionPresence::present, end, end}, positions)
            .startPosition;
    const auto point = text_layout_point(text, stop, core::CaretAffinity::trailing);
    if (!point)
    {
        text_failure_ = RenderFailure::directwrite;
        return;
    }
    brush_->SetColor(detail::to_color(core::line_break_mark_color(frame.palette)));
    InvisibleMarkPainter::paint_line_break(
        *context_.Get(), brush_.Get(),
        LineBreakPlace{D2D1::Point2F(static_cast<float>(area.left) + point.value().x,
                                     static_cast<float>(area.top) + point.value().y),
                       shown.line_break.value(), height, space.value()});
}
} // namespace nenenib::ui::win32
