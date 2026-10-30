#include "PreviewSelectionText.hpp"

#include "PreviewBlockKind.hpp"
#include "VimCharacterBoundary.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] bool selectable(const PreviewBlock &block) noexcept
{
    return block.kind != PreviewBlockKind::rule;
}

[[nodiscard]] std::size_t shown_size(const MarkdownDocument &document, std::size_t block) noexcept
{
    return preview_shown_text(document.blocks.at(block)).size();
}

// block の後ろ・前で最初の選べる要素（無ければ nullopt）。
[[nodiscard]] std::optional<std::size_t> next_selectable(const MarkdownDocument &document,
                                                         std::size_t block) noexcept
{
    for (auto at = block + 1; at < document.blocks.size(); ++at)
    {
        if (selectable(document.blocks.at(at)))
        {
            return at;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> previous_selectable(const MarkdownDocument &document,
                                                             std::size_t block) noexcept
{
    for (auto at = block; at > 0; --at)
    {
        if (selectable(document.blocks.at(at - 1)))
        {
            return at - 1;
        }
    }
    return std::nullopt;
}

[[nodiscard]] PreviewPoint next_character(const MarkdownDocument &document,
                                          PreviewPoint point) noexcept
{
    const auto shown = preview_shown_text(document.blocks.at(point.block));
    if (point.offset < shown.size())
    {
        return PreviewPoint{point.block, vim_character_end(shown, Offset{point.offset}).value};
    }
    const auto next = next_selectable(document, point.block);
    return next.has_value() ? PreviewPoint{next.value(), 0} : point;
}

[[nodiscard]] PreviewPoint previous_character(const MarkdownDocument &document,
                                              PreviewPoint point) noexcept
{
    if (point.offset > 0)
    {
        const auto shown = preview_shown_text(document.blocks.at(point.block));
        return PreviewPoint{point.block, vim_character_start(shown, Offset{point.offset}).value};
    }
    const auto previous = previous_selectable(document, point.block);
    return previous.has_value()
               ? PreviewPoint{previous.value(), shown_size(document, previous.value())}
               : point;
}

[[nodiscard]] PreviewPoint block_above(const MarkdownDocument &document,
                                       PreviewPoint point) noexcept
{
    if (point.offset > 0)
    {
        return PreviewPoint{point.block, 0};
    }
    const auto previous = previous_selectable(document, point.block);
    return previous.has_value() ? PreviewPoint{previous.value(), 0} : point;
}

[[nodiscard]] PreviewPoint block_below(const MarkdownDocument &document,
                                       PreviewPoint point) noexcept
{
    const auto end = shown_size(document, point.block);
    if (point.offset < end)
    {
        return PreviewPoint{point.block, end};
    }
    const auto next = next_selectable(document, point.block);
    return next.has_value() ? PreviewPoint{next.value(), shown_size(document, next.value())}
                            : point;
}
} // namespace

std::string_view preview_shown_text(const PreviewBlock &block) noexcept
{
    std::string_view text = block.text;
    const bool monospaced =
        block.kind == PreviewBlockKind::code || block.kind == PreviewBlockKind::html;
    if (monospaced && text.ends_with('\n'))
    {
        text.remove_suffix(1);
    }
    return text;
}

std::size_t preview_anchor_block(const MarkdownDocument &document, Offset anchor)
{
    const auto after = std::upper_bound(document.blocks.begin(), document.blocks.end(), anchor,
                                        [](Offset offset, const PreviewBlock &block)
                                        { return offset < block.source_begin; });
    const auto count = static_cast<std::size_t>(std::distance(document.blocks.begin(), after));
    return count == 0 ? 0 : count - 1;
}

PreviewPoint preview_selection_origin(const PreviewSelection &selection) noexcept
{
    return PreviewPoint{selection.block_begin, selection.offset_begin};
}

PreviewPoint preview_selection_head(const PreviewSelection &selection) noexcept
{
    return PreviewPoint{selection.block_end, selection.offset_end};
}

PreviewSelection preview_selection_of(PreviewPoint origin, PreviewPoint head) noexcept
{
    return PreviewSelection{origin.block, origin.offset, head.block, head.offset};
}

PreviewPoint preview_point_clamped(const MarkdownDocument &document, PreviewPoint point) noexcept
{
    if (document.blocks.empty())
    {
        return PreviewPoint{0, 0};
    }
    const auto block = std::min(point.block, document.blocks.size() - 1);
    return PreviewPoint{block, std::min(point.offset, shown_size(document, block))};
}

std::optional<std::pair<PreviewPoint, PreviewPoint>>
preview_selection_range(const MarkdownDocument &document,
                        const PreviewSelection &selection) noexcept
{
    if (document.blocks.empty())
    {
        return std::nullopt;
    }
    auto first = preview_point_clamped(document, preview_selection_origin(selection));
    auto last = preview_point_clamped(document, preview_selection_head(selection));
    if (first == last)
    {
        return std::nullopt;
    }
    if (preview_point_before(last, first))
    {
        std::swap(first, last);
    }
    return std::pair{first, last};
}

std::optional<std::pair<std::size_t, std::size_t>>
preview_selected_bytes(const MarkdownDocument &document, const PreviewSelection &selection,
                       std::size_t block) noexcept
{
    const auto range = preview_selection_range(document, selection);
    if (!range.has_value() || block < range.value().first.block ||
        block > range.value().second.block || !selectable(document.blocks.at(block)))
    {
        return std::nullopt;
    }
    const auto first = range.value().first;
    const auto last = range.value().second;
    const std::size_t begin = block == first.block ? first.offset : 0;
    const std::size_t end = block == last.block ? last.offset : shown_size(document, block);
    if (begin >= end)
    {
        return std::nullopt;
    }
    return std::pair{begin, end};
}

std::string preview_selection_text(const MarkdownDocument &document,
                                   const PreviewSelection &selection)
{
    const auto range = preview_selection_range(document, selection);
    if (!range.has_value())
    {
        return {};
    }
    const auto first = range.value().first;
    const auto last = range.value().second;
    std::string text;
    bool joined = false;
    for (auto block = first.block; block <= last.block; ++block)
    {
        const auto &element = document.blocks.at(block);
        if (!selectable(element))
        {
            continue;
        }
        if (joined)
        {
            text.push_back('\n');
        }
        joined = true;
        const auto visible = preview_shown_text(element);
        const std::size_t begin = block == first.block ? first.offset : 0;
        const std::size_t end = block == last.block ? last.offset : visible.size();
        text.append(visible.substr(begin, end - begin));
    }
    return text;
}

std::optional<PreviewSelection> preview_selection_all(const MarkdownDocument &document) noexcept
{
    if (document.blocks.empty())
    {
        return std::nullopt;
    }
    const auto last = document.blocks.size() - 1;
    return PreviewSelection{0, 0, last, shown_size(document, last)};
}

PreviewPoint preview_point_moved(const MarkdownDocument &document, PreviewPoint point,
                                 CaretMotion motion) noexcept
{
    if (document.blocks.empty())
    {
        return PreviewPoint{0, 0};
    }
    const auto at = preview_point_clamped(document, point);
    const auto last = document.blocks.size() - 1;
    switch (motion)
    {
    case CaretMotion::previous_character:
        return previous_character(document, at);
    case CaretMotion::next_character:
        return next_character(document, at);
    case CaretMotion::previous_line:
        return block_above(document, at);
    case CaretMotion::next_line:
        return block_below(document, at);
    case CaretMotion::line_start:
        return PreviewPoint{at.block, 0};
    case CaretMotion::line_end:
        return PreviewPoint{at.block, shown_size(document, at.block)};
    case CaretMotion::document_start:
        return PreviewPoint{0, 0};
    case CaretMotion::document_end:
        return PreviewPoint{last, shown_size(document, last)};
    case CaretMotion::previous_word:
    case CaretMotion::next_word:
    case CaretMotion::page_up:
    case CaretMotion::page_down:
        return at;
    }
    std::unreachable();
}
} // namespace nenenib::core
