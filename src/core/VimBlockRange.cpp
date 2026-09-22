#include "VimBlockRange.hpp"

#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimBlockEdit.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::size_t single_step = 1;

[[nodiscard]] LineNumber line_of(const TextBuffer &text, Offset at)
{
    return text.position_of(at).line;
}

// 矩形の左右の桁。角の文字の「最初の桁」と「最後の桁」で挟む＝端に掛かった Tab や全角も
// 矩形の中に入る（Vim の getvvcol と同じ・Issue #112 で実測）。
[[nodiscard]] std::size_t left_column_of(const TextBuffer &text, const OffsetRange &ordered)
{
    return std::min(virtual_column(text, ordered.begin).value,
                    virtual_column(text, ordered.end).value);
}

[[nodiscard]] std::size_t right_column_of(const TextBuffer &text, const OffsetRange &ordered)
{
    return std::max(virtual_column_end(text, ordered.begin).value,
                    virtual_column_end(text, ordered.end).value);
}

// `$` の右端。覆う行のうちいちばん長い行の内容の終わりまで（Vim は取るときにこの幅へ畳む）。
[[nodiscard]] std::size_t longest_line(const TextBuffer &text, LineNumber first, LineNumber last)
{
    std::size_t longest = 0;
    for (std::size_t number = first.value; number <= last.value; ++number)
    {
        longest = std::max(longest, virtual_width(text.line_text(LineNumber{number})));
    }
    return longest;
}

// 矩形が覆う桁の数。右が左より手前に来る（`$` で全部空行）ときも 1 桁として数える。
[[nodiscard]] std::size_t block_columns(std::size_t left, std::size_t right) noexcept
{
    return right >= left ? right - left + single_step : single_step;
}

[[nodiscard]] VimBlockLine empty_line_at(Offset at, std::size_t lead)
{
    return VimBlockLine{OffsetRange{at, at}, OffsetRange{at, at}, lead, 0, 0, 0};
}

// 1 行ぶんの形（決定 2）。行が矩形より手前で終わればレジスタに幅ぶんの空白だけが入り、本文は
// 動かない。端が文字の途中に掛かればその文字は丸ごと `range` に入り、掛かった桁は空白になる。
[[nodiscard]] VimBlockLine block_line(const TextBuffer &text, LineNumber number, std::size_t left,
                                      std::size_t right)
{
    const std::string line = text.line_text(number);
    const Offset start = text.line_start(number);
    const std::size_t columns = virtual_width(line);
    if (left > columns + single_step)
    {
        return empty_line_at(text.line_end(number), block_columns(left, right));
    }
    const std::size_t last_column = std::min(right, columns);
    if (left > last_column)
    {
        return empty_line_at(Offset{start.value + byte_at_column(line, VirtualColumn{left})}, 0);
    }
    const std::size_t first_byte = byte_at_column(line, VirtualColumn{left});
    const std::size_t last_byte = byte_at_column(line, VirtualColumn{last_column});
    const std::size_t first_begins = column_of(line, first_byte).value;
    const std::size_t first_ends = column_end_of(line, first_byte).value;
    const std::size_t last_begins = column_of(line, last_byte).value;
    const std::size_t last_ends = column_end_of(line, last_byte).value;
    const std::size_t after_last = next_code_point(line, Offset{last_byte}).value;
    const OffsetRange range{Offset{start.value + first_byte}, Offset{start.value + after_last}};
    if (first_byte == last_byte && first_begins < left && last_ends > right)
    {
        // 矩形が 1 つの文字（Tab か全角）の内側に収まる。覆った桁だけが空白としてレジスタへ。
        return VimBlockLine{range,
                            OffsetRange{range.begin, range.begin},
                            right - left + single_step,
                            0,
                            left - first_begins,
                            last_ends - right};
    }
    const bool cut_left = first_begins < left;
    const bool cut_right = last_ends > right;
    const std::size_t inside_begin =
        cut_left ? next_code_point(line, Offset{first_byte}).value : first_byte;
    const std::size_t inside_end = std::max(inside_begin, cut_right ? last_byte : after_last);
    return VimBlockLine{
        range,
        OffsetRange{Offset{start.value + inside_begin}, Offset{start.value + inside_end}},
        cut_left ? first_ends - left + single_step : 0,
        cut_right ? right - last_begins + single_step : 0,
        cut_left ? left - first_begins : 0,
        cut_right ? last_ends - right : 0};
}
} // namespace

VimBlockRange vim_block_range(const TextBuffer &text, const Selection &selection,
                              VimColumnWish wish)
{
    const OffsetRange ordered = selection_range(selection);
    const LineNumber first = line_of(text, ordered.begin);
    const LineNumber last = line_of(text, ordered.end);
    const std::size_t left = left_column_of(text, ordered);
    std::size_t right = right_column_of(text, ordered);
    switch (wish)
    {
    case VimColumnWish::at_line_end:
        right = longest_line(text, first, last);
        break;
    case VimColumnWish::at_column:
        break;
    }
    std::vector<VimBlockLine> lines;
    lines.reserve(last.value - first.value + single_step);
    for (std::size_t number = first.value; number <= last.value; ++number)
    {
        lines.push_back(block_line(text, LineNumber{number}, left, right));
    }
    return VimBlockRange{std::move(lines), first, VirtualColumn{left},
                         VimBlockWidth{block_columns(left, right)}};
}

VimBlockRange vim_block_range_for(const TextBuffer &text, const Selection &selection,
                                  const std::optional<VimWantedColumn> &wanted)
{
    return vim_block_range(text, selection,
                           wanted.has_value() ? wanted.value().wish : VimColumnWish::at_column);
}

VimRemoveBlock vim_remove_block(const VimBlockRange &block)
{
    std::vector<VimBlockEdit> edits;
    for (const VimBlockLine &line : block.lines)
    {
        if (is_empty(line.range))
        {
            continue;
        }
        edits.push_back(
            VimBlockEdit{line.range, std::string(line.keep_lead + line.keep_tail, ' ')});
    }
    const VimBlockLine &first = block.lines.front();
    return VimRemoveBlock{std::move(edits), Offset{first.range.begin.value + first.keep_lead}};
}

std::string vim_block_text(const TextBuffer &text, const VimBlockRange &block)
{
    std::string joined;
    for (std::size_t index = 0; index < block.lines.size(); ++index)
    {
        if (index > 0)
        {
            joined += '\n';
        }
        const VimBlockLine &line = block.lines.at(index);
        joined.append(line.lead, ' ');
        joined += text.text_range(line.inside.begin, line.inside.end);
        joined.append(line.tail, ' ');
    }
    return joined;
}
} // namespace nenenib::core
