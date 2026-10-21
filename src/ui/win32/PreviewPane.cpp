#include "PreviewPane.hpp"

#include "PreviewBlock.hpp"
#include "PreviewPainter.hpp"
#include "PreviewScene.hpp"

#include <algorithm>
#include <iterator>

namespace nenenib::ui::win32
{
float preview_scroll_of(const core::MarkdownDocument &document, const PreviewBlockTops &tops,
                        core::Offset anchor)
{
    const auto after = std::upper_bound(document.blocks.begin(), document.blocks.end(), anchor,
                                        [](core::Offset offset, const core::PreviewBlock &block)
                                        { return offset < block.source_begin; });
    const auto count = static_cast<std::size_t>(std::distance(document.blocks.begin(), after));
    if (count <= 1 || count - 1 >= tops.size())
    {
        return 0.0F;
    }
    return tops.top(count - 1);
}

std::expected<const PreviewBlockTops *, RenderFailure>
PreviewPane::remeasure(const std::shared_ptr<const core::MarkdownDocument> &document,
                       const PreviewTextStyle &style, float width)
{
    if (tops_.has_value() && measured_ == document && measured_style_.has_value() &&
        measured_style_.value() == style && measured_width_ == width)
    {
        return &tops_.value();
    }
    tops_.reset();
    measured_ = nullptr;
    auto tops = PreviewBlockTops::measure(*document, layouts_, style, width);
    if (!tops)
    {
        return std::unexpected(tops.error());
    }
    measured_ = document;
    measured_style_ = style;
    measured_width_ = width;
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
    const auto measured = remeasure(document, style, static_cast<float>(core::width_of(area)));
    if (!measured)
    {
        return std::unexpected(measured.error());
    }
    const PreviewBlockTops &tops = *measured.value();
    std::optional<RenderFailure> failure;
    PreviewPainter<ID2D1RenderTarget> painter(target, layouts_, brushes_, failure);
    painter.paint(PreviewScene{*document, tops, style, area,
                               preview_scroll_of(*document, tops, frame.anchor)});
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
