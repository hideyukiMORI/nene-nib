#include "PreviewBlockTops.hpp"

#include "PreviewBlockKind.hpp"
#include "PreviewMetrics.hpp"
#include "PreviewTaskBox.hpp"
#include "RenderFailure.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
// 記号の種類（無い・箇条書き・番号）。
using MarkerKind = std::uint8_t;
constexpr MarkerKind no_marker = 0;
constexpr MarkerKind bullet_marker = 1;
constexpr MarkerKind number_marker = 2;
constexpr std::string_view bullet = "•";
// チェックボックスの記号の組の長さ（記号の文字の長さと重ならない値・ADR 0155 の追補 23）。
constexpr std::size_t task_marker_length = static_cast<std::size_t>(-1);
// 上限で読みを止めたリストの番号の記号（5 桁）。
constexpr std::string_view capped_number = "99999.";

[[nodiscard]] MarkerKind marker_kind(const core::PreviewBlock &block) noexcept
{
    if (block.marker.empty())
    {
        return no_marker;
    }
    return block.marker == bullet ? bullet_marker : number_marker;
}

// 深さごとの記号の種類に block の記号を覚える。
void remember(std::vector<MarkerKind> &kinds, const core::PreviewBlock &block)
{
    const auto kind = marker_kind(block);
    if (kind != no_marker && block.list_depth > 0)
    {
        kinds.resize(block.list_depth, no_marker);
        kinds.at(block.list_depth - 1) = kind;
    }
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
    remember(kinds, next);
    return same;
}

// 錨の 1 つ前の要素の記号だけを種類の始めにする（それより前は読まない・#813）。
[[nodiscard]] std::vector<MarkerKind> seeded_kinds(const core::MarkdownDocument &document,
                                                   std::size_t first)
{
    std::vector<MarkerKind> kinds;
    if (first > 0)
    {
        remember(kinds, document.blocks.at(first - 1));
    }
    return kinds;
}

[[nodiscard]] float indent_of(const core::PreviewBlock &block, const std::vector<float> &columns,
                              std::uint32_t dpi)
{
    float indent = preview_quote_left(block.quote_depth, dpi);
    for (std::size_t depth = 0; depth < block.list_depth; ++depth)
    {
        indent += depth < columns.size() ? columns.at(depth) : preview_list_indent(dpi);
    }
    return indent;
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

// タスクの項目（記号を描く項目のうちチェックボックスを持つもの）か。
[[nodiscard]] bool task_item(const core::PreviewBlock &block) noexcept
{
    return block.kind == core::PreviewBlockKind::list_item && block.task.has_value();
}

// 錨の要素の上端（最初の要素のときだけ上の余白）。
[[nodiscard]] float first_top(std::size_t first, const PreviewTextStyle &style)
{
    return first == 0 ? preview_top_margin(style.dpi) : 0.0F;
}
} // namespace

std::expected<PreviewBlockTops, RenderFailure>
PreviewBlockTops::measure(const core::MarkdownDocument &document, PreviewTextLayouts &layouts,
                          const PreviewTextStyle &style, const PreviewViewport &viewport)
{
    PreviewBlockTops tops;
    tops.first_ = std::min(viewport.first, document.blocks.size());
    tops.kinds_ = seeded_kinds(document, tops.first_);
    for (;;)
    {
        const auto placed = tops.place(document, layouts, style, viewport);
        if (!placed)
        {
            return std::unexpected(placed.error());
        }
        if (placed.value())
        {
            tops.lists_.clear();
            return tops;
        }
    }
}

std::expected<bool, RenderFailure> PreviewBlockTops::place(const core::MarkdownDocument &document,
                                                           PreviewTextLayouts &layouts,
                                                           const PreviewTextStyle &style,
                                                           const PreviewViewport &viewport)
{
    tops_.clear();
    bottoms_.clear();
    indents_.clear();
    task_boxes_.clear();
    float y = first_top(first_, style);
    for (auto index = first_; index < document.blocks.size() && y < viewport.height; ++index)
    {
        const auto &block = document.blocks.at(index);
        y += gap_before(document, index, style);
        const auto columns = columns_at(document, index, layouts, style);
        if (!columns)
        {
            return std::unexpected(columns.error());
        }
        if (columns.value().second)
        {
            return false;
        }
        const auto indent = indent_of(block, *columns.value().first, style.dpi);
        const auto height = text_height_of(
            block, layouts, style, preview_text_width(block, viewport.width, indent, style.dpi));
        if (!height)
        {
            return std::unexpected(height.error());
        }
        indents_.push_back(indent);
        task_boxes_.push_back(task_item(block) ? std::optional(preview_task_box(indent, y, style))
                                               : std::nullopt);
        tops_.push_back(y);
        y += preview_extent(block.kind, height.value(), style.dpi);
        bottoms_.push_back(y);
    }
    return true;
}

std::size_t PreviewBlockTops::start_of(const core::MarkdownDocument &document, std::size_t index)
{
    while (first_ + starts_.size() <= index)
    {
        const auto next = first_ + starts_.size();
        const bool same =
            next > 0 && continues(document.blocks.at(next - 1), document.blocks.at(next), kinds_);
        starts_.push_back(same && !starts_.empty() ? starts_.back() : next);
    }
    return starts_.at(index - first_);
}

float PreviewBlockTops::gap_before(const core::MarkdownDocument &document, std::size_t index,
                                   const PreviewTextStyle &style)
{
    if (index == first_)
    {
        return 0.0F;
    }
    const bool same = start_of(document, index) == start_of(document, index - 1);
    return preview_gap(document.blocks.at(index - 1), document.blocks.at(index), style, same);
}

std::expected<std::pair<const PreviewBlockTops::Columns *, bool>, RenderFailure>
PreviewBlockTops::columns_at(const core::MarkdownDocument &document, std::size_t index,
                             PreviewTextLayouts &layouts, const PreviewTextStyle &style)
{
    const auto start = start_of(document, index);
    const auto columns = columns_for(document, start, layouts, style);
    if (!columns)
    {
        return std::unexpected(columns.error());
    }
    auto &list = *columns.value();
    const auto widened = widen(list, document.blocks.at(index), layouts, style);
    if (!widened)
    {
        return std::unexpected(widened.error());
    }
    // 同じリストの前の要素はもう狭い列で置いたので、錨から置き直す。
    return std::pair<const Columns *, bool>{&list.first, widened.value() && start < index};
}

std::expected<PreviewBlockTops::ListColumns *, RenderFailure>
PreviewBlockTops::columns_for(const core::MarkdownDocument &document, std::size_t start,
                              PreviewTextLayouts &layouts, const PreviewTextStyle &style)
{
    const auto found =
        std::ranges::find(lists_, start, &std::pair<std::size_t, ListColumns>::first);
    if (found != lists_.end())
    {
        return &found->second;
    }
    const auto end = list_end(document, start);
    const auto items =
        std::span<const core::PreviewBlock>{document.blocks}.subspan(start, end - start);
    auto &list = lists_.emplace_back(start, ListColumns{}).second;
    const auto widened = widen_all(list, items, layouts, style);
    if (!widened)
    {
        return std::unexpected(widened.error());
    }
    // 上限で止めてもリストが続くなら、番号の列は 5 桁ぶん。
    if (end - start == list_scan_limit && end < document.blocks.size() &&
        start_of(document, end) == start)
    {
        const auto capped = widen_capped(list, items, layouts, style);
        if (!capped)
        {
            return std::unexpected(capped.error());
        }
    }
    return &list;
}

std::size_t PreviewBlockTops::list_end(const core::MarkdownDocument &document, std::size_t start)
{
    auto end = start + 1;
    // リストの外の要素は次を読まない（枠の下の要素を読まない・#811 のレビュー R1）。
    if (document.blocks.at(start).list_depth == 0)
    {
        return end;
    }
    while (end < document.blocks.size() && end - start < list_scan_limit &&
           start_of(document, end) == start)
    {
        ++end;
    }
    return end;
}

std::expected<void, RenderFailure>
PreviewBlockTops::widen_all(ListColumns &list, std::span<const core::PreviewBlock> items,
                            PreviewTextLayouts &layouts, const PreviewTextStyle &style)
{
    for (const auto &item : items)
    {
        const auto widened = widen(list, item, layouts, style);
        if (!widened)
        {
            return std::unexpected(widened.error());
        }
    }
    return {};
}

std::expected<void, RenderFailure>
PreviewBlockTops::widen_capped(ListColumns &list, std::span<const core::PreviewBlock> items,
                               PreviewTextLayouts &layouts, const PreviewTextStyle &style)
{
    for (const auto &item : items)
    {
        const MarkerKey key{item.list_depth, capped_number.size()};
        if (marker_kind(item) != number_marker || std::ranges::contains(list.second, key))
        {
            continue;
        }
        auto widest = item;
        widest.marker = std::string(capped_number);
        const auto widened = widen(list, widest, layouts, style);
        if (!widened)
        {
            return std::unexpected(widened.error());
        }
    }
    return {};
}

// 記号の幅は深さと文字の長さごとに 1 つだけ測る（番号の数字は等幅なので、同じ桁数の記号は同じ
// 幅・長いリストでも layout は桁数ぶん）。
std::expected<bool, RenderFailure> PreviewBlockTops::widen(ListColumns &list,
                                                           const core::PreviewBlock &block,
                                                           PreviewTextLayouts &layouts,
                                                           const PreviewTextStyle &style)
{
    auto &[columns, measured] = list;
    columns.resize(std::max<std::size_t>(columns.size(), block.list_depth),
                   preview_list_indent(style.dpi));
    // チェックボックスは記号の文字を測らず、一辺の幅を記号の幅とする（ADR 0155 の追補 23）。
    const bool task = task_item(block);
    const MarkerKey key{block.list_depth, task ? task_marker_length : block.marker.size()};
    if (block.marker.empty() || block.list_depth == 0 || std::ranges::contains(measured, key))
    {
        return false;
    }
    measured.push_back(key);
    const auto width =
        task ? std::expected<float, RenderFailure>(preview_task_side(style))
             : layouts.advance(preview_marker_block(block), preview_marker_layout_width(), style);
    if (!width)
    {
        return std::unexpected(width.error());
    }
    auto &column = columns.at(block.list_depth - 1U);
    const auto wider = std::max(column, width.value() + preview_marker_gap(style.dpi));
    const bool widened = wider > column;
    column = wider;
    return widened;
}

std::size_t PreviewBlockTops::first() const noexcept
{
    return first_;
}

std::size_t PreviewBlockTops::first_visible(float scroll) const
{
    return first_ + static_cast<std::size_t>(std::distance(
                        bottoms_.begin(), std::ranges::upper_bound(bottoms_, scroll)));
}

float PreviewBlockTops::top(std::size_t index) const
{
    return tops_.at(index - first_);
}

float PreviewBlockTops::bottom(std::size_t index) const
{
    return bottoms_.at(index - first_);
}

float PreviewBlockTops::indent(std::size_t index) const
{
    return indents_.at(index - first_);
}

const std::optional<PreviewTaskBox> &PreviewBlockTops::task_box(std::size_t index) const
{
    return task_boxes_.at(index - first_);
}

std::size_t PreviewBlockTops::size() const noexcept
{
    return first_ + tops_.size();
}

std::size_t PreviewBlockTops::scanned() const noexcept
{
    return starts_.size() + (first_ > 0 ? 1U : 0U);
}
} // namespace nenenib::ui::win32
