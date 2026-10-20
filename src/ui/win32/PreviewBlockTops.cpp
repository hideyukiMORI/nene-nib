#include "PreviewBlockTops.hpp"

#include "PreviewPlacement.hpp"

#include <algorithm>
#include <iterator>

namespace nenenib::ui::win32
{
namespace
{
// 要素の layout の高さ（水平線は文字を持たないので 0）。
[[nodiscard]] std::expected<float, RenderFailure> text_height_of(const core::PreviewBlock &block,
                                                                 PreviewTextLayouts &layouts,
                                                                 const PreviewTextStyle &style,
                                                                 float frame_width)
{
    if (block.kind == core::PreviewBlockKind::rule)
    {
        return 0.0F;
    }
    return layouts.measure(block, preview_text_width(block, frame_width, style.dpi), style);
}
} // namespace

std::expected<PreviewBlockTops, RenderFailure>
PreviewBlockTops::measure(const core::MarkdownDocument &document, PreviewTextLayouts &layouts,
                          const PreviewTextStyle &style, float frame_width)
{
    PreviewBlockTops tops;
    tops.tops_.reserve(document.blocks.size());
    tops.bottoms_.reserve(document.blocks.size());
    const auto margin = preview_top_margin(style.dpi);
    float y = margin;
    const core::PreviewBlock *previous = nullptr;
    for (const auto &block : document.blocks)
    {
        y += previous == nullptr ? 0.0F : preview_gap(*previous, block, style);
        const auto text_height = text_height_of(block, layouts, style, frame_width);
        if (!text_height)
        {
            return std::unexpected(text_height.error());
        }
        tops.tops_.push_back(y);
        y += preview_extent(block.kind, text_height.value(), style.dpi);
        tops.bottoms_.push_back(y);
        previous = &block;
    }
    tops.total_ = y + margin;
    return tops;
}

std::size_t PreviewBlockTops::first_visible(float scroll) const
{
    return static_cast<std::size_t>(
        std::distance(bottoms_.begin(), std::ranges::upper_bound(bottoms_, scroll)));
}

float PreviewBlockTops::top(std::size_t index) const
{
    return tops_.at(index);
}

float PreviewBlockTops::bottom(std::size_t index) const
{
    return bottoms_.at(index);
}

std::size_t PreviewBlockTops::size() const noexcept
{
    return tops_.size();
}

float PreviewBlockTops::total() const noexcept
{
    return total_;
}
} // namespace nenenib::ui::win32
