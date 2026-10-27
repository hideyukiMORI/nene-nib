#include "PreviewPane.hpp"

#include "PreviewBlock.hpp"
#include "PreviewPainter.hpp"
#include "PreviewScene.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

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
    theme_ = theme.name;
    return {};
}

std::expected<void, RenderFailure> PreviewPane::draw_scene(ID2D1RenderTarget &target,
                                                           const PreviewPictureKey &key,
                                                           const core::LayoutRect &frame)
{
    const PreviewViewport viewport{key.anchor, static_cast<float>(key.width),
                                   static_cast<float>(key.height)};
    const auto measured = remeasure(key.document, key.style, viewport);
    if (!measured)
    {
        return std::unexpected(measured.error());
    }
    ++painted_;
    std::optional<RenderFailure> failure;
    PreviewPainter<ID2D1RenderTarget> painter(target, layouts_, brushes_, failure);
    painter.paint(PreviewScene{*key.document, *measured.value(), key.style, frame, 0.0F});
    if (failure.has_value())
    {
        return std::unexpected(failure.value());
    }
    return {};
}

ID2D1Bitmap1 *PreviewPane::picture_of(ID2D1DeviceContext &context, std::int32_t width,
                                      std::int32_t height)
{
    if (width <= 0 || height <= 0)
    {
        return nullptr;
    }
    const auto size = D2D1::SizeU(static_cast<UINT32>(width), static_cast<UINT32>(height));
    if (picture_ != nullptr && picture_->GetPixelSize().width == size.width &&
        picture_->GetPixelSize().height == size.height)
    {
        return picture_.Get();
    }
    picture_.Reset();
    float dpi_x = 0.0F;
    float dpi_y = 0.0F;
    context.GetDpi(&dpi_x, &dpi_y);
    // 描く先と同じ画素形式と alpha・同じ DPI（製品は 96 で 1 DIP = 1 物理画素）。
    const auto properties =
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, context.GetPixelFormat(), dpi_x, dpi_y);
    if (FAILED(context.CreateBitmap(size, nullptr, 0, properties, &picture_)))
    {
        picture_.Reset();
        return nullptr;
    }
    ++pictures_;
    return picture_.Get();
}

std::expected<void, RenderFailure> PreviewPane::record(ID2D1DeviceContext &context,
                                                       ID2D1Bitmap1 &picture, PreviewPictureKey key)
{
    Microsoft::WRL::ComPtr<ID2D1Image> previous;
    context.GetTarget(&previous);
    context.SetTarget(&picture);
    context.Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    const auto drawn = draw_scene(context, key, core::LayoutRect{0, 0, key.width, key.height});
    context.SetTarget(previous.Get());
    if (!drawn)
    {
        return drawn;
    }
    pictured_.emplace(std::move(key));
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
    const auto width = core::width_of(area);
    const auto height = core::height_of(area);
    PreviewPictureKey key{&target, document, preview_anchor_block(*document, frame.anchor),
                          width,   height,   style,
                          theme_};
    // 座標は物理画素の整数なので、拡大縮小せず最近傍で写せば滲まない。
    const auto destination =
        D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                    static_cast<float>(area.right), static_cast<float>(area.bottom));
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> context;
    if (FAILED(target.QueryInterface(IID_PPV_ARGS(&context))))
    {
        pictured_.reset();
        return draw_scene(target, key, area);
    }
    if (pictured_.has_value() && pictured_.value() == key && picture_ != nullptr)
    {
        ++reused_;
        context->DrawBitmap(picture_.Get(), destination, 1.0F,
                            D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, nullptr, nullptr);
        return {};
    }
    pictured_.reset();
    auto *const picture = picture_of(*context.Get(), width, height);
    if (picture == nullptr)
    {
        return draw_scene(target, key, area);
    }
    const auto recorded = record(*context.Get(), *picture, std::move(key));
    if (!recorded)
    {
        return recorded;
    }
    context->DrawBitmap(picture, destination, 1.0F, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                        nullptr, nullptr);
    return {};
}

std::size_t PreviewPane::created() const noexcept
{
    return layouts_.created();
}

std::size_t PreviewPane::painted() const noexcept
{
    return painted_;
}

std::size_t PreviewPane::reused() const noexcept
{
    return reused_;
}

std::size_t PreviewPane::pictures() const noexcept
{
    return pictures_;
}
} // namespace nenenib::ui::win32
