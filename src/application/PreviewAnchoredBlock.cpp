#include "PreviewAnchoredBlock.hpp"

#include "PreviewSelectionText.hpp"

namespace nenenib::application
{
std::size_t preview_anchored_block(const core::MarkdownDocument &document, core::Offset anchor,
                                   std::optional<std::size_t> block)
{
    if (block.has_value() && block.value() < document.blocks.size())
    {
        return block.value();
    }
    return core::preview_anchor_block(document, anchor);
}
} // namespace nenenib::application
