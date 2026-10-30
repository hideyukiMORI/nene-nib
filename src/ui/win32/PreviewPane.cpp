#include "PreviewPane.hpp"

#include "PreviewBlock.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewMetrics.hpp"
#include "PreviewPainter.hpp"
#include "PreviewScene.hpp"
#include "PreviewSelectionText.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
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
    painter.paint(
        PreviewScene{*key.document, *measured.value(), key.style, frame, 0.0F, key.selection});
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
    area_.reset();
    if (document == nullptr)
    {
        target.FillRectangle(
            D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                        static_cast<float>(area.right), static_cast<float>(area.bottom)),
            brushes_.of(PreviewInk::background));
        return {};
    }
    area_ = area;
    const auto width = core::width_of(area);
    const auto height = core::height_of(area);
    PreviewPictureKey key{
        &target, document,       core::preview_anchor_block(*document, frame.anchor),
        width,   height,         style,
        theme_,  frame.selection};
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

std::optional<std::size_t> PreviewPane::task_at(std::int32_t x, std::int32_t y) const
{
    if (!area_.has_value() || !tops_.has_value() || !core::contains(area_.value(), x, y))
    {
        return std::nullopt;
    }
    const auto &area = area_.value();
    const auto &tops = tops_.value();
    for (auto index = tops.first(); index < tops.size(); ++index)
    {
        const auto &box = tops.task_box(index);
        if (box.has_value() && core::contains(box.value().target, x - area.left, y - area.top))
        {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<core::PreviewPoint> PreviewPane::point_at(std::int32_t x, std::int32_t y)
{
    if (!area_.has_value() || !tops_.has_value() || measured_ == nullptr ||
        tops_.value().first() >= tops_.value().size())
    {
        return std::nullopt;
    }
    const auto &area = area_.value();
    const auto &tops = tops_.value();
    const auto local = static_cast<float>(y - area.top);
    std::size_t index = tops.first();
    for (auto at = tops.first(); at < tops.size() && tops.top(at) <= local; ++at)
    {
        index = at;
    }
    const auto &block = measured_->blocks.at(index);
    if (local < tops.top(index) || block.kind == core::PreviewBlockKind::rule)
    {
        return core::PreviewPoint{index, 0};
    }
    if (local >= tops.bottom(index))
    {
        return core::PreviewPoint{index, core::preview_shown_text(block).size()};
    }
    return hit_block(tops, area, index, D2D1::Point2F(static_cast<float>(x - area.left), local));
}

std::optional<core::PreviewPoint> PreviewPane::hit_block(const PreviewBlockTops &tops,
                                                         const core::LayoutRect &area,
                                                         std::size_t index, D2D1_POINT_2F point)
{
    if (!measured_style_.has_value())
    {
        return std::nullopt;
    }
    const auto &style = measured_style_.value();
    const auto &block = measured_->blocks.at(index);
    // 描く所（PreviewPainter::draw_text）と同じ幅と原点。コードブロックは面の内側の余白だけ下げる。
    const auto inset =
        block.kind == core::PreviewBlockKind::code ? preview_code_padding(style.dpi) : 0.0F;
    const auto width = preview_text_width(block, static_cast<float>(core::width_of(area)),
                                          tops.indent(index), style.dpi);
    const auto layout = layouts_.get(block, width, style);
    if (!layout)
    {
        return std::nullopt;
    }
    BOOL trailing = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics{};
    if (FAILED(layout.value()->HitTestPoint(point.x - tops.indent(index) - inset,
                                            point.y - tops.top(index) - inset, &trailing, &inside,
                                            &metrics)))
    {
        return std::nullopt;
    }
    const auto units = metrics.textPosition + (trailing != FALSE ? metrics.length : 0U);
    return core::PreviewPoint{index, preview_byte_of_units(core::preview_shown_text(block), units)};
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
