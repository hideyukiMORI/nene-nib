#include "PreviewPane.hpp"

#include "PreviewBlock.hpp"
#include "PreviewPainter.hpp"
#include "PreviewScene.hpp"

#include <algorithm>
#include <iterator>

namespace nenenib::ui::win32
{
namespace
{
[[nodiscard]] bool same_viewport(const PreviewViewport &left, const PreviewViewport &right) noexcept
{
    return left.first == right.first && left.width == right.width && left.height == right.height;
}
} // namespace

std::size_t preview_anchor_block(const core::MarkdownDocument &document, core::Offset anchor)
{
    const auto after = std::upper_bound(document.blocks.begin(), document.blocks.end(), anchor,
                                        [](core::Offset offset, const core::PreviewBlock &block)
                                        { return offset < block.source_begin; });
    const auto count = static_cast<std::size_t>(std::distance(document.blocks.begin(), after));
    return count == 0 ? 0 : count - 1;
}

std::expected<const PreviewBlockTops *, RenderFailure>
PreviewPane::remeasure(const std::shared_ptr<const core::MarkdownDocument> &document,
                       const PreviewTextStyle &style, const PreviewViewport &viewport)
{
    if (tops_.has_value() && measured_ == document && measured_style_.has_value() &&
        measured_style_.value() == style && same_viewport(measured_viewport_, viewport))
    {
        return &tops_.value();
    }
    tops_.reset();
    measured_ = nullptr;
    auto tops = PreviewBlockTops::measure(*document, layouts_, style, viewport);
    if (!tops)
    {
        return std::unexpected(tops.error());
    }
    measured_ = document;
    measured_style_ = style;
    measured_viewport_ = viewport;
    return &tops_.emplace(std::move(tops).value());
}

std::expected<void, RenderFailure> PreviewPane::prepare(ID2D1RenderTarget &target,
                                                        const core::Theme &theme)
{
    if (FAILED(brushes_.prepare(target, theme)))
    {
        return std::unexpected(RenderFailure::direct2d);
    }
    return {};
}

std::expected<void, RenderFailure> PreviewPane::paint(ID2D1RenderTarget &target,
                                                      const application::PreviewFrame &frame,
                                                      const PreviewTextStyle &style,
                                                      const core::LayoutRect &area)
{
    const auto &document = frame.document;
    if (document == nullptr)
    {
        target.FillRectangle(
            D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                        static_cast<float>(area.right), static_cast<float>(area.bottom)),
            brushes_.of(PreviewInk::background));
        return {};
    }
    const PreviewViewport viewport{preview_anchor_block(*document, frame.anchor),
                                   static_cast<float>(core::width_of(area)),
                                   static_cast<float>(core::height_of(area))};
    const auto measured = remeasure(document, style, viewport);
    if (!measured)
    {
        return std::unexpected(measured.error());
    }
    const PreviewBlockTops &tops = *measured.value();
    std::optional<RenderFailure> failure;
    PreviewPainter<ID2D1RenderTarget> painter(target, layouts_, brushes_, failure);
    painter.paint(PreviewScene{*document, tops, style, area, 0.0F});
    if (failure.has_value())
    {
        return std::unexpected(failure.value());
    }
    return {};
}

std::size_t PreviewPane::created() const noexcept
{
    return layouts_.created();
}
} // namespace nenenib::ui::win32
