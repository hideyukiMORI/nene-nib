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
    // 置く節のバイトの範囲（UTF-16 の位置は最後に前から 1 回だけ数えて写す・#1485）。
    std::vector<PreviewByteRange> bytes;
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
        bytes.emplace_back(at->begin.value, at->end.value);
        placed.push_back(
            PreviewInlineImage{PreviewInlineBox{0, 0, size.width, size.height}, std::move(pixels)});
        covered = at->end.value;
    }
    if (placed.empty())
    {
        return placed;
    }
    const auto ranges =
        preview_unit_ranges(core::preview_shown_text(document.blocks.at(block)), bytes);
    for (std::size_t index = 0; index < placed.size(); ++index)
    {
        placed.at(index).box.begin = ranges.at(index).startPosition;
        placed.at(index).box.length = ranges.at(index).length;
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

std::vector<std::optional<D2D1_POINT_2F>>
preview_inline_origins(IDWriteTextLayout &layout, std::span<const PreviewInlineImage> inlines,
                       std::uint32_t dpi)
{
    std::vector<std::optional<D2D1_POINT_2F>> origins(inlines.size());
    if (inlines.empty())
    {
        return origins;
    }
    UINT32 count = 0;
    const auto counted = layout.GetLineMetrics(nullptr, 0, &count);
    std::vector<DWRITE_LINE_METRICS> lines(count);
    if ((FAILED(counted) && counted != E_NOT_SUFFICIENT_BUFFER) ||
        FAILED(layout.GetLineMetrics(lines.data(), count, &count)))
    {
        return origins;
    }
    // 行ごとの終わり（UTF-16 の位置）と上端（前の行の高さを順に足す）。
    std::vector<UINT32> ends;
    std::vector<float> tops;
    ends.reserve(lines.size());
    tops.reserve(lines.size());
    UINT32 start = 0;
    float top = 0.0F;
    for (const auto &line : lines)
    {
        tops.push_back(top);
        start += line.length;
        ends.push_back(start);
        top += line.height;
    }
    for (std::size_t index = 0; index < inlines.size(); ++index)
    {
        const auto box = inlines[index].box;
        FLOAT x = 0.0F;
        FLOAT y = 0.0F;
        DWRITE_HIT_TEST_METRICS hit{};
        // 箱を含む最初の行（箱の始まり < 行の終わり）。
        const auto row = std::ranges::upper_bound(ends, box.begin);
        if (row == ends.end() || FAILED(layout.HitTestTextPosition(box.begin, FALSE, &x, &y, &hit)))
        {
            continue;
        }
        const auto line = static_cast<std::size_t>(std::distance(ends.begin(), row));
        origins.at(index) = D2D1::Point2F(x, tops.at(line) + lines.at(line).baseline -
                                                 preview_inline_extent(box.height, dpi));
    }
    return origins;
}
} // namespace nenenib::ui::win32
