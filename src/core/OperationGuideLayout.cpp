#include "OperationGuideLayout.hpp"

#include "DevicePixels.hpp"

#include <algorithm>
#include <optional>

namespace nenenib::core
{
namespace
{
[[nodiscard]] std::optional<BodyGuideLayout> body_guide(const BodyLayout &body,
                                                        std::uint32_t dpi) noexcept
{
    const auto width = to_pixels(228, dpi);
    const auto left = body.band.left + (width_of(body.band) - width) / 2;
    const auto top = std::max(body.band.top + height_of(body.band) / 3,
                              body.content.top + body.line_height + to_pixels(12, dpi));
    if (left < body.band.left + to_pixels(16, dpi) ||
        left + width > body.band.right - to_pixels(16, dpi) ||
        top + to_pixels(120 + 12, dpi) > body.band.bottom)
    {
        return std::nullopt;
    }
    BodyGuideLayout result{};
    for (std::size_t index = 0; index < result.rows.size(); ++index)
    {
        const auto row_top = top + to_pixels(static_cast<std::int32_t>(index) * 32, dpi);
        const auto bottom = row_top + to_pixels(20, dpi);
        result.rows.at(index) =
            GuideRowLayout{LayoutRect{left, row_top, left + to_pixels(64, dpi), bottom},
                           LayoutRect{left + to_pixels(84, dpi), row_top, left + width, bottom}};
    }
    result.note =
        LayoutRect{left, top + to_pixels(104, dpi), left + width, top + to_pixels(120, dpi)};
    return result;
}

[[nodiscard]] OperationGuideLayout status_guide(const StatusBarLayout &status,
                                                std::uint32_t dpi) noexcept
{
    const auto right = status.items.front().left - to_pixels(28, dpi);
    if (right - (status.mode.right + to_pixels(16, dpi)) < to_pixels(160, dpi))
    {
        return std::monostate{};
    }
    const auto left = right - to_pixels(148, dpi);
    const auto top = status.band.top;
    const auto bottom = status.band.bottom;
    return StatusGuideLayout{
        {GuideRowLayout{
             LayoutRect{left, top, left + to_pixels(48, dpi), bottom},
             LayoutRect{left + to_pixels(52, dpi), top, left + to_pixels(76, dpi), bottom}},
         GuideRowLayout{
             LayoutRect{right - to_pixels(56, dpi), top, right - to_pixels(40, dpi), bottom},
             LayoutRect{right - to_pixels(36, dpi), top, right, bottom}}}};
}
} // namespace

OperationGuideLayout operation_guide_layout(GuideContext context, const BodyLayout &body,
                                            const StatusBarLayout &status,
                                            std::uint32_t dpi) noexcept
{
    switch (context)
    {
    case GuideContext::hidden:
        return std::monostate{};
    case GuideContext::untouched_untitled:
        if (const auto layout = body_guide(body, dpi); layout.has_value())
        {
            return layout.value();
        }
        return status_guide(status, dpi);
    case GuideContext::other:
        return status_guide(status, dpi);
    }
    return std::monostate{};
}
} // namespace nenenib::core
