#include "PreviewInlineImages.hpp"

#include "DevicePixels.hpp"
#include "ImageGeometry.hpp"
#include "ImageSize.hpp"
#include "MarkdownImage.hpp"
#include "PreviewMetrics.hpp"
#include "PreviewSelectionText.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace nenenib::ui::win32
{
std::vector<PreviewInlineImage> preview_inline_images(const core::MarkdownDocument &document,
                                                      const application::PreviewImages *images,
                                                      std::size_t block, std::uint32_t frame_dips)
{
    std::vector<PreviewInlineImage> placed;
    if (images == nullptr)
    {
        return placed;
    }
    const auto &all = document.images;
    const auto shown = core::preview_shown_text(document.blocks.at(block));
    // 置いた最後の箱の終わり（要素の文字の中のバイト位置）。入れ子の中の節はこれより前で始まる。
    std::size_t covered = 0;
    for (auto at = std::ranges::lower_bound(all, block, {}, &core::MarkdownImage::block);
         at != all.end() && at->block == block; ++at)
    {
        const auto ordinal = static_cast<std::size_t>(std::distance(all.begin(), at));
        auto pixels = images->pixels_of(ordinal);
        const auto size = pixels == nullptr ? core::ImageSize{0, 0}
                                            : core::image_placement(pixels->original, frame_dips);
        if (size.width == 0 || size.height == 0 || at->begin.value < covered)
        {
            continue;
        }
        const auto begin = preview_units(shown, at->begin.value);
        const auto end = preview_units(shown, at->end.value);
        placed.push_back(PreviewInlineImage{
            PreviewInlineBox{begin, end > begin ? end - begin : 0, size.width, size.height},
            std::move(pixels)});
        covered = at->end.value;
    }
    return placed;
}

std::uint32_t preview_dips_of(float pixels, std::uint32_t dpi) noexcept
{
    if (dpi == 0 || pixels <= 0.0F)
    {
        return 0;
    }
    return static_cast<std::uint32_t>(
        std::floor(pixels * static_cast<float>(core::reference_dpi) / static_cast<float>(dpi)));
}

float preview_inline_extent(std::uint32_t dips, std::uint32_t dpi) noexcept
{
    return std::max(1.0F, std::round(preview_pixels(static_cast<float>(dips), dpi)));
}

std::optional<D2D1_POINT_2F> preview_inline_origin(IDWriteTextLayout &layout, PreviewInlineBox box,
                                                   std::uint32_t dpi)
{
    FLOAT x = 0.0F;
    FLOAT y = 0.0F;
    DWRITE_HIT_TEST_METRICS hit{};
    UINT32 count = 0;
    const auto counted = layout.GetLineMetrics(nullptr, 0, &count);
    if (FAILED(layout.HitTestTextPosition(box.begin, FALSE, &x, &y, &hit)) ||
        (FAILED(counted) && counted != E_NOT_SUFFICIENT_BUFFER))
    {
        return std::nullopt;
    }
    std::vector<DWRITE_LINE_METRICS> lines(count);
    if (FAILED(layout.GetLineMetrics(lines.data(), count, &count)))
    {
        return std::nullopt;
    }
    const auto height = preview_inline_extent(box.height, dpi);
    float top = 0.0F;
    UINT32 start = 0;
    for (const auto &line : lines)
    {
        if (box.begin < start + line.length)
        {
            return D2D1::Point2F(x, top + line.baseline - height);
        }
        start += line.length;
        top += line.height;
    }
    return std::nullopt;
}
} // namespace nenenib::ui::win32
