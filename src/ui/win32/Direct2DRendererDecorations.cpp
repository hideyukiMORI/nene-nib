// 本文の飾りの層（ADR 0193 の決定 1・2・6・#1975）。application が行に置いた桁の欄を、保持した
// layout の外に矩形で重ねる。段 1 はインデントガイドだけで、段 2（不可視文字）と段 3（括弧）は
// draw_decorations_under と、本文の字の後に描く上の飾りの 1 本へ足す。オフの道は frame の初めの
// 真偽 1 つと、行ごとの欄の有無の読み 1 つだけ。
#include "Direct2DRenderer.hpp"

#include "DevicePixels.hpp"
#include "EditorFeature.hpp"
#include "FeatureCosts.hpp"
#include "IndentGuide.hpp"
#include "IndentGuidePainter.hpp"
#include "IndentGuideRow.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
namespace
{
// 線の太さ（DIP・見本の 1 DIP と強める段の 2 DIP）。
constexpr std::int32_t guide_dips = 1;
constexpr std::int32_t strong_guide_dips = 2;
} // namespace

void Direct2DRenderer::begin_decorations(const application::EditorFrame &frame)
{
    guide_paint_.reset();
    if (core::feature_enabled(frame.settings, core::EditorFeature::indent_guides))
    {
        guide_paint_ =
            core::indent_guide_paint(frame.settings.indent_guide_style,
                                     frame.settings.indent_guide_colors, frame.body, frame.palette);
    }
}

void Direct2DRenderer::draw_decorations_under(const BodyPane &pane, const core::LayoutRect &area,
                                              const application::LineView &line)
{
    draw_indent_guides(pane, area, line);
}

void Direct2DRenderer::draw_indent_guides(const BodyPane &pane, const core::LayoutRect &area,
                                          const application::LineView &line)
{
    if (!line.indent_guides.has_value() || !guide_paint_.has_value())
    {
        return;
    }
    const auto space = body_text_.space_width(core::DisplayForm::text);
    if (!space.has_value())
    {
        return;
    }
    const IndentGuideRow row{line.indent_guides.value(),
                             static_cast<float>(area.left),
                             static_cast<float>(area.top),
                             static_cast<float>(pane.body.line_height),
                             space.value(),
                             static_cast<float>(core::to_pixels(guide_dips, dpi_)),
                             static_cast<float>(core::to_pixels(strong_guide_dips, dpi_))};
    IndentGuidePainter::paint(*context_.Get(), brush_.Get(), row, guide_paint_.value());
}
} // namespace nenenib::ui::win32
