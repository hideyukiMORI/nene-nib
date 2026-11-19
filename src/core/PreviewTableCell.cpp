#include "PreviewTableCell.hpp"

#include "Offset.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewLink.hpp"
#include "PreviewSpan.hpp"
#include "PreviewStyle.hpp"
#include "PreviewTableRow.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
// 升 [first, second) の中のリンク（#1869 の段 L2）。升に始まりと終わりが収まるものだけを升の位置へ
// 写す。
[[nodiscard]] std::vector<PreviewLink> cell_links(const PreviewBlock &row, std::size_t first,
                                                  std::size_t second)
{
    std::vector<PreviewLink> links;
    for (const auto &link : row.links)
    {
        if (link.begin.value >= first && link.end.value <= second &&
            link.end.value > link.begin.value)
        {
            links.push_back(PreviewLink{Offset{link.begin.value - first},
                                        Offset{link.end.value - first}, link.target});
        }
    }
    return links;
}
} // namespace

std::pair<std::size_t, std::size_t> preview_table_cell_range(const PreviewBlock &row,
                                                             std::size_t column)
{
    if (!row.table.has_value() || column >= row.table.value().cells.size())
    {
        return {row.text.size(), row.text.size()};
    }
    const auto &cells = row.table.value().cells;
    const auto first = std::min(cells.at(column).value, row.text.size());
    // 次の升の始まりの 1 つ前が升の間の Tab。
    const auto second = column + 1U < cells.size()
                            ? std::max(first, cells.at(column + 1U).value - 1U)
                            : row.text.size();
    return {first, std::min(second, row.text.size())};
}

PreviewBlock preview_table_cell(const PreviewBlock &row, std::size_t column)
{
    const auto [first, second] = preview_table_cell_range(row, column);
    std::vector<PreviewSpan> spans;
    const bool header = row.table.has_value() && row.table.value().header;
    if (header && second > first)
    {
        spans.push_back(PreviewSpan{Offset{0U}, Offset{second - first}, PreviewStyle::strong});
    }
    for (const auto &span : row.spans)
    {
        const auto begin = std::max(span.begin.value, first);
        const auto end = std::min(span.end.value, second);
        if (span.begin.value >= first && span.begin.value <= second && end >= begin)
        {
            spans.push_back(PreviewSpan{Offset{begin - first}, Offset{end - first}, span.style});
        }
    }
    auto links = cell_links(row, first, second);
    return PreviewBlock{.kind = PreviewBlockKind::table_row,
                        .level = 0U,
                        .quote_depth = 0U,
                        .list_depth = 0U,
                        .marker = {},
                        .source_begin = Offset{0U},
                        .text = row.text.substr(first, second - first),
                        .spans = std::move(spans),
                        .task = std::nullopt,
                        .table = std::nullopt,
                        .alert = std::nullopt,
                        .code_colors = std::nullopt,
                        .links = std::move(links),
                        .piece = PreviewCodePiece::only};
}

namespace
{
constexpr std::uint64_t fnv_offset = 14695981039346656037ULL;
constexpr std::uint64_t fnv_prime = 1099511628211ULL;

void mixed(std::uint64_t &digest, std::uint64_t value) noexcept
{
    for (int shift = 0; shift < 64; shift += 8)
    {
        digest = (digest ^ ((value >> shift) & 0xFFU)) * fnv_prime;
    }
}
} // namespace

std::uint64_t preview_table_digest(std::span<const PreviewBlock> rows) noexcept
{
    std::uint64_t digest = fnv_offset;
    for (const auto &row : rows)
    {
        mixed(digest, row.text.size());
        for (const char value : row.text)
        {
            digest = (digest ^ static_cast<unsigned char>(value)) * fnv_prime;
        }
        mixed(digest, row.table.has_value() && row.table.value().header ? 1U : 0U);
        for (const auto &span : row.spans)
        {
            mixed(digest, span.begin.value);
            mixed(digest, span.end.value);
            mixed(digest, static_cast<std::uint64_t>(span.style));
        }
    }
    return digest;
}

std::pair<std::size_t, std::size_t> preview_table_rows(const MarkdownDocument &document,
                                                       std::size_t index)
{
    const auto &blocks = document.blocks;
    const auto row_at = [&blocks](std::size_t at) -> std::optional<std::uint32_t>
    {
        return at < blocks.size() ? blocks.at(at).table.transform([](const PreviewTableRow &table)
                                                                  { return table.row; })
                                  : std::nullopt;
    };
    const auto row = row_at(index);
    if (!row.has_value())
    {
        return {index, std::min(index + 1U, blocks.size())};
    }
    const std::size_t first = row.value() <= index ? index - row.value() : 0U;
    auto last = index + 1U;
    for (auto next = row.value() + 1U; row_at(last) == next; ++next)
    {
        ++last;
    }
    return {first, last};
}
} // namespace nenenib::core
