// 本文の飾りの層（ADR 0193 の決定 1・2・6・#1975）。application が行に置いた桁の欄を、保持した
// layout の外に矩形で重ねる。段 1 はインデントガイドだけで、段 2（不可視文字）と段 3（括弧）は
// draw_decorations_under と、本文の字の後に描く上の飾りの 1 本へ足す。オフの道は frame の初めの
// 真偽 1 つと、行ごとの欄の有無の読み 1 つだけ。
#include "Direct2DRenderer.hpp"

#include "IndentGuidePainter.hpp"
#include "IndentGuideRow.hpp"

namespace nenenib::ui::win32
{
void Direct2DRenderer::begin_decorations(const application::EditorFrame &frame)
{
    guide_paint_.begin(frame.settings, frame.body, frame.palette);
}

void Direct2DRenderer::draw_decorations_under(const BodyPane &pane, const core::LayoutRect &area,
                                              const application::LineView &line)
{
    draw_indent_guides(pane, area, line);
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
} // namespace nenenib::ui::win32
