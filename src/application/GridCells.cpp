#include "GridCells.hpp"

#include "DisplayWidth.hpp"
#include "Grid.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"
#include "VirtualColumnChunk.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <string>

namespace nenenib::application
{
namespace
{
constexpr char32_t tab_character = U'\t';
// 1 文字を読むのに足りる長さ（UTF-8 の最長）。
constexpr std::size_t longest_sequence = 4;
// 累積だけを数えるときの的（どの文字も占めない升）。
constexpr std::size_t unreachable_cell = std::numeric_limits<std::size_t>::max();

[[nodiscard]] constexpr core::VirtualColumnCursor zero_cursor() noexcept
{
    return core::VirtualColumnCursor{0, 0, 0, 0, 0};
}

// 1 文字を置いて state を進め、その文字が始まる升を返す。送りは格子の 1 本（grid_placed_cell）。
std::size_t placed(core::VirtualColumnCursor &state, char32_t value, std::size_t columns) noexcept
{
    const auto advance = core::column_advance(value, state.column);
    const auto start =
        value == tab_character ? state.cell : core::grid_placed_cell(state.cell, advance, columns);
    state.column += advance;
    state.cell = start + advance;
    ++state.code_point;
    return start;
}

// plain の部分は 1 バイト＝1 桁＝1 升＝1 コードポイント。
void added(core::VirtualColumnCursor &state, std::size_t bytes) noexcept
{
    state.column += bytes;
    state.cell += bytes;
    state.code_point += bytes;
}

[[nodiscard]] core::GridPoint point_at(std::size_t cell, std::size_t columns) noexcept
{
    return core::GridPoint{cell / columns, cell % columns};
}
} // namespace

GridCells::GridCells(const core::VirtualColumnIndex &index, const core::TextBuffer &text,
                     std::size_t columns) noexcept
    : index_(index), text_(text), columns_(std::max<std::size_t>(columns, 1))
{
}

std::size_t GridCells::columns() const noexcept
{
    return columns_;
}

std::size_t GridCells::scanned() const noexcept
{
    return scanned_;
}

// at を含む塊（at が本文の末尾なら最後の塊）。塊が無ければ 0（呼ぶ側の繰り返しが回らない）。
std::size_t GridCells::first_chunk(std::size_t at) const noexcept
{
    const auto chunks = index_.chunks();
    const auto found = std::ranges::upper_bound(chunks, at, {}, &core::VirtualColumnChunk::start);
    const auto position = static_cast<std::size_t>(std::distance(chunks.begin(), found));
    return position == 0 ? 0 : position - 1;
}

core::OffsetRange GridCells::part_of(std::size_t chunk, std::size_t begin,
                                     std::size_t stop) const noexcept
{
    const auto &part = index_.chunks()[chunk];
    const auto from = std::max(part.start, begin);
    const auto to = std::max(std::min(part.start + part.bytes, stop), from);
    return core::OffsetRange{core::Offset{from}, core::Offset{to}};
}

std::optional<std::size_t> GridCells::reached(core::OffsetRange range, bool plain,
                                              core::VirtualColumnCursor &state, std::size_t target)
{
    const auto length = range.end.value - range.begin.value;
    if (plain)
    {
        if (target < state.cell + length)
        {
            return range.begin.value + (target - state.cell);
        }
        added(state, length);
        return std::nullopt;
    }
    ++scanned_;
    const auto bytes = text_.text_range(range.begin, range.end);
    for (std::size_t inside = 0; inside < bytes.size();
         inside = core::next_code_point(bytes, core::Offset{inside}).value)
    {
        const auto start =
            placed(state, core::code_point_at(bytes, core::Offset{inside}), columns_);
        if (state.cell > start && target < state.cell)
        {
            return range.begin.value + inside;
        }
    }
    return std::nullopt;
}

core::VirtualColumnCursor GridCells::cursor_at(core::LineNumber line, core::Offset at)
{
    const auto begin = text_.line_start(line).value;
    const auto stop = std::clamp(at.value, begin, text_.line_end(line).value);
    const auto chunks = index_.chunks();
    auto state = zero_cursor();
    for (auto chunk = first_chunk(begin); chunk < chunks.size() && chunks[chunk].start < stop;
         ++chunk)
    {
        const auto range = part_of(chunk, begin, stop);
        if (range.begin != range.end)
        {
            static_cast<void>(reached(range, chunks[chunk].plain, state, unreachable_cell));
        }
    }
    return state;
}

core::Offset GridCells::previous_in_line(core::LineNumber line, core::Offset at) const
{
    const auto begin = text_.line_start(line).value;
    const auto from = std::max(begin, at.value - std::min(at.value, longest_sequence));
    const auto before = text_.text_range(core::Offset{from}, at);
    if (before.empty())
    {
        return at;
    }
    return core::Offset{from +
                        core::previous_code_point(before, core::Offset{before.size()}).value};
}

core::GridPoint GridCells::point_of(core::LineNumber line, core::Offset at)
{
    const auto cursor = cursor_at(line, at);
    if (at.value >= text_.line_end(line).value)
    {
        const auto cell = cursor.cell;
        return cell > 0 && cell % columns_ == 0 ? core::GridPoint{(cell / columns_) - 1, columns_}
                                                : point_at(cell, columns_);
    }
    const auto peeked = text_.text_range(at, core::Offset{at.value + longest_sequence});
    const char32_t value = core::code_point_at(peeked, core::Offset{0});
    if (core::display_width(value) == core::DisplayWidth::zero && value != tab_character)
    {
        const auto base = previous_in_line(line, at);
        return base == at ? point_at(cursor.cell, columns_) : point_of(line, base);
    }
    const auto cells = core::column_advance(value, cursor.column);
    const auto start =
        value == tab_character ? cursor.cell : core::grid_placed_cell(cursor.cell, cells, columns_);
    return point_at(start, columns_);
}

core::Offset GridCells::byte_of(core::LineNumber line, core::GridPoint point)
{
    const auto target = (point.row * columns_) + point.cell;
    const auto begin = text_.line_start(line).value;
    const auto end = text_.line_end(line).value;
    const auto chunks = index_.chunks();
    auto state = zero_cursor();
    for (auto chunk = first_chunk(begin); chunk < chunks.size() && chunks[chunk].start < end;
         ++chunk)
    {
        const auto range = part_of(chunk, begin, end);
        const auto found = range.begin == range.end
                               ? std::nullopt
                               : reached(range, chunks[chunk].plain, state, target);
        if (found.has_value())
        {
            return core::Offset{found.value()};
        }
    }
    return core::Offset{end};
}

std::size_t GridCells::rows(core::LineNumber line)
{
    return core::grid_rows(cursor_at(line, text_.line_end(line)).cell, columns_);
}
} // namespace nenenib::application
