#include "PreviewBlockTops.hpp"

#include "PreviewPlacement.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
// 記号の種類（無い・箇条書き・番号）。
using MarkerKind = std::uint8_t;
using Columns = std::vector<float>;
constexpr MarkerKind no_marker = 0;
constexpr MarkerKind bullet_marker = 1;
constexpr MarkerKind number_marker = 2;
constexpr std::string_view bullet = "•";

[[nodiscard]] MarkerKind marker_kind(const core::PreviewBlock &block) noexcept
{
    if (block.marker.empty())
    {
        return no_marker;
    }
    return block.marker == bullet ? bullet_marker : number_marker;
}

// next が前の要素と同じリストの続きか。kinds は今のリストの深さごとの記号の種類で、同じ深さで
// 種類が変われば別のリスト（箇条書き → 番号つき）。浅い項目が来れば深い段の種類を忘れる。
[[nodiscard]] bool continues(const core::PreviewBlock &previous, const core::PreviewBlock &next,
                             std::vector<MarkerKind> &kinds)
{
    const auto kind = marker_kind(next);
    const std::size_t depth = next.list_depth;
    const bool switched = kind != no_marker && depth > 0 && kinds.size() >= depth &&
                          kinds.at(depth - 1) != no_marker && kinds.at(depth - 1) != kind;
    const bool same = previous.list_depth > 0 && depth > 0 &&
                      previous.quote_depth == next.quote_depth && !switched;
    if (!same)
    {
        kinds.clear();
    }
    if (kind != no_marker && depth > 0)
    {
        kinds.resize(depth, no_marker);
        kinds.at(depth - 1) = kind;
    }
    return same;
}

// 要素ごとの、属するリストの最初の要素の位置（リストの外の要素は自分）。
[[nodiscard]] std::vector<std::size_t> list_starts(const core::MarkdownDocument &document)
{
    std::vector<std::size_t> starts(document.blocks.size());
    std::vector<MarkerKind> kinds;
    for (std::size_t index = 0; index < starts.size(); ++index)
    {
        const bool same =
            index > 0 && continues(document.blocks.at(index - 1), document.blocks.at(index), kinds);
        starts.at(index) = same ? starts.at(index - 1) : index;
    }
    return starts;
}

// [first, second) のリストの深さごとの記号の列の幅。
[[nodiscard]] std::expected<Columns, RenderFailure>
columns_of(const core::MarkdownDocument &document, std::pair<std::size_t, std::size_t> range,
           PreviewTextLayouts &layouts, const PreviewTextStyle &style)
{
    const auto least = preview_list_indent(style.dpi);
    Columns columns;
    for (auto index = range.first; index < range.second; ++index)
    {
        const auto &block = document.blocks.at(index);
        columns.resize(std::max<std::size_t>(columns.size(), block.list_depth), least);
        if (block.marker.empty() || block.list_depth == 0)
        {
            continue;
        }
        const auto width =
            layouts.advance(preview_marker_block(block), preview_marker_layout_width(), style);
        if (!width)
        {
            return std::unexpected(width.error());
        }
        auto &column = columns.at(block.list_depth - 1U);
        column = std::max(column, width.value() + preview_marker_gap(style.dpi));
    }
    return columns;
}

[[nodiscard]] float indent_of(const core::PreviewBlock &block, const Columns &columns,
                              std::uint32_t dpi)
{
    float indent = preview_quote_left(block.quote_depth, dpi);
    for (std::size_t depth = 0; depth < block.list_depth; ++depth)
    {
        indent += depth < columns.size() ? columns.at(depth) : preview_list_indent(dpi);
    }
    return indent;
}

// 要素ごとの字下げ。リストは最初の要素で記号の列を測り、同じリストの要素で揃える。
[[nodiscard]] std::expected<std::vector<float>, RenderFailure>
indents_of(const core::MarkdownDocument &document, const std::vector<std::size_t> &starts,
           PreviewTextLayouts &layouts, const PreviewTextStyle &style)
{
    std::vector<float> indents(starts.size());
    for (std::size_t begin = 0; begin < starts.size();)
    {
        // starts は減らないので、次のリストは begin より大きい最初の値から。
        const auto end = static_cast<std::size_t>(
            std::distance(starts.begin(), std::ranges::upper_bound(starts, begin)));
        const auto columns = columns_of(document, {begin, end}, layouts, style);
        if (!columns)
        {
            return std::unexpected(columns.error());
        }
        for (auto index = begin; index < end; ++index)
        {
            indents.at(index) = indent_of(document.blocks.at(index), columns.value(), style.dpi);
        }
        begin = end;
    }
    return indents;
}

// 要素の layout の高さ（水平線は文字を持たないので 0）。
[[nodiscard]] std::expected<float, RenderFailure> text_height_of(const core::PreviewBlock &block,
                                                                 PreviewTextLayouts &layouts,
                                                                 const PreviewTextStyle &style,
                                                                 float width)
{
    if (block.kind == core::PreviewBlockKind::rule)
    {
        return 0.0F;
    }
    return layouts.measure(block, width, style);
}
} // namespace

std::expected<PreviewBlockTops, RenderFailure>
PreviewBlockTops::measure(const core::MarkdownDocument &document, PreviewTextLayouts &layouts,
                          const PreviewTextStyle &style, float frame_width)
{
    const auto starts = list_starts(document);
    auto indents = indents_of(document, starts, layouts, style);
    if (!indents)
    {
        return std::unexpected(indents.error());
    }
    PreviewBlockTops tops;
    tops.indents_ = std::move(indents.value());
    const auto margin = preview_top_margin(style.dpi);
    float y = margin;
    for (std::size_t index = 0; index < starts.size(); ++index)
    {
        const auto &block = document.blocks.at(index);
        y += index == 0 ? 0.0F
                        : preview_gap(document.blocks.at(index - 1), block, style,
                                      starts.at(index) == starts.at(index - 1));
        const auto height = text_height_of(
            block, layouts, style,
            preview_text_width(block, frame_width, tops.indents_.at(index), style.dpi));
        if (!height)
        {
            return std::unexpected(height.error());
        }
        tops.tops_.push_back(y);
        y += preview_extent(block.kind, height.value(), style.dpi);
        tops.bottoms_.push_back(y);
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

float PreviewBlockTops::indent(std::size_t index) const
{
    return indents_.at(index);
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
