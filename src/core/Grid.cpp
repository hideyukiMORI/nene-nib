#include "Grid.hpp"

#include "DisplayWidth.hpp"
#include "LineEnding.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"
#include "VirtualColumnIndex.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
// 1 文字と、その後ろの 1 バイト（CRLF の判定）を読むのに足りる長さ。
constexpr std::size_t peek_bytes = 5;
constexpr std::size_t longest_sequence = 4;
constexpr char32_t tab_character = U'\t';

[[nodiscard]] GridPoint point_of(std::size_t cell, std::size_t columns) noexcept
{
    return GridPoint{cell / columns, cell % columns};
}

// at が行の内容の終わり（本文の末尾・'\n'・CRLF の '\r'）か。
[[nodiscard]] bool at_line_end(const TextBuffer &text, std::string_view peeked) noexcept
{
    if (peeked.empty() || peeked.front() == '\n')
    {
        return true;
    }
    return peeked.front() == '\r' && peeked.size() > 1 && peeked.at(1) == '\n' &&
           text.line_ending() == LineEnding::crlf;
}

// at の直前の code point の先頭。at が行頭なら at。
[[nodiscard]] Offset previous_in_line(const TextBuffer &text, Offset at)
{
    const auto from = at.value - std::min(at.value, longest_sequence);
    const auto before = text.text_range(Offset{from}, at);
    if (before.empty() || before.back() == '\n')
    {
        return at;
    }
    return Offset{from + previous_code_point(before, Offset{before.size()}).value};
}
} // namespace

std::size_t grid_rows(std::size_t cells, std::size_t columns) noexcept
{
    columns = std::max<std::size_t>(columns, 1);
    return std::max<std::size_t>((cells + columns - 1) / columns, 1);
}

std::size_t grid_placed_cell(std::size_t cell, std::size_t cells, std::size_t columns) noexcept
{
    if (columns == 0 || cells < 2 || cells > columns)
    {
        return cell;
    }
    const auto room = columns - (cell % columns);
    return cells > room ? cell + room : cell;
}

GridPoint grid_cell_of(const VirtualColumnIndex &index, const TextBuffer &text, Offset at)
{
    const auto columns = std::max<std::size_t>(index.columns(), 1);
    const auto peeked = text.text_range(at, Offset{at.value + peek_bytes});
    const auto cursor = index.cursor_at(text, at);
    if (at_line_end(text, peeked))
    {
        const auto cell = cursor.cell;
        return cell > 0 && cell % columns == 0 ? GridPoint{(cell / columns) - 1, columns}
                                               : point_of(cell, columns);
    }
    const char32_t value = code_point_at(peeked, Offset{0});
    if (display_width(value) == DisplayWidth::zero && value != tab_character)
    {
        const auto base = previous_in_line(text, at);
        return base == at ? point_of(cursor.cell, columns) : grid_cell_of(index, text, base);
    }
    const auto cells = column_advance(value, cursor.column);
    const auto start =
        value == tab_character ? cursor.cell : grid_placed_cell(cursor.cell, cells, columns);
    return point_of(start, columns);
}

Offset grid_byte_of(const VirtualColumnIndex &index, const TextBuffer &text, LineNumber line,
                    GridPoint point)
{
    const auto columns = std::max<std::size_t>(index.columns(), 1);
    return index.byte_at_cell(text, line, (point.row * columns) + point.cell);
}
} // namespace nenenib::core
