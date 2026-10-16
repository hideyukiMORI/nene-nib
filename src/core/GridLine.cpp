#include "GridLine.hpp"

#include "DisplayWidth.hpp"
#include "Offset.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace nenenib::core
{
namespace
{
// 累積を残す塊の大きさ（決定 4・改行の索引の chunk と同じ桁）。
constexpr std::size_t chunk_bytes = 64UL * 1024UL;
constexpr std::size_t tab_stop = 8;
constexpr unsigned char first_plain = 0x20;
constexpr unsigned char last_plain = 0x7E;
constexpr char32_t tab_character = U'\t';
constexpr float half = 0.5F;

[[nodiscard]] bool plain_bytes(std::string_view bytes) noexcept
{
    return std::ranges::all_of(bytes,
                               [](char value)
                               {
                                   const auto byte = static_cast<unsigned char>(value);
                                   return byte >= first_plain && byte <= last_plain;
                               });
}

[[nodiscard]] std::size_t cells_of(DisplayWidth width) noexcept
{
    switch (width)
    {
    case DisplayWidth::zero:
        return 0;
    case DisplayWidth::single:
        return 1;
    case DisplayWidth::wide:
        return 2;
    case DisplayWidth::unprintable:
        return 6;
    case DisplayWidth::hex:
        return 4;
    }
    std::unreachable();
}

// Tab 以外の 1 文字を置き、始まる升を返す。画面行の残りに収まらない 2 升以上の文字は次の行へ送る。
std::size_t place(GridCheckpoint &state, std::size_t cells, std::size_t columns) noexcept
{
    if (cells == 0)
    {
        return state.last_start;
    }
    const auto room = columns - (state.cell % columns);
    if (cells > 1 && cells <= columns && cells > room)
    {
        state.cell += room;
    }
    const auto start = state.cell;
    state.virtual_column += cells;
    state.cell += cells;
    return start;
}

// 1 文字ぶん進め、その文字が始まる升を返す。
std::size_t step(std::string_view text, GridCheckpoint &state, std::size_t columns) noexcept
{
    const char32_t value = code_point_at(text, Offset{state.byte});
    std::size_t start = state.cell;
    if (value == tab_character)
    {
        const auto advance = tab_stop - (state.virtual_column % tab_stop);
        state.virtual_column += advance;
        state.cell += advance;
    }
    else
    {
        start = place(state, cells_of(display_width(value)), columns);
    }
    state.last_start = start;
    state.byte = next_code_point(text, Offset{state.byte}).value;
    ++state.column;
    return start;
}

// ASCII だけの塊の中を count 文字ぶん割り算で進める。
void jump(GridCheckpoint &state, std::size_t count) noexcept
{
    if (count == 0)
    {
        return;
    }
    state.byte += count;
    state.column += count;
    state.virtual_column += count;
    state.cell += count;
    state.last_start = state.cell - 1;
}
} // namespace

GridLine::GridLine(std::string text, std::size_t columns)
    : text_(std::move(text)), columns_(std::max<std::size_t>(columns, 1))
{
}

GridLine GridLine::build(std::string text, std::size_t columns)
{
    GridLine line(std::move(text), columns);
    const std::string_view bytes = line.text_;
    line.checkpoints_.reserve((bytes.size() / chunk_bytes) + 1);
    GridCheckpoint state{};
    while (state.byte < bytes.size())
    {
        const auto end = std::min(state.byte + chunk_bytes, bytes.size());
        state.plain = plain_bytes(bytes.substr(state.byte, end - state.byte));
        line.checkpoints_.push_back(state);
        if (state.plain)
        {
            jump(state, end - state.byte);
            continue;
        }
        while (state.byte < end)
        {
            static_cast<void>(step(bytes, state, line.columns_));
        }
    }
    state.plain = false;
    line.end_ = state;
    return line;
}

std::string_view GridLine::text() const noexcept
{
    return text_;
}

std::size_t GridLine::columns() const noexcept
{
    return columns_;
}

std::size_t GridLine::cells() const noexcept
{
    return end_.cell;
}

std::size_t GridLine::code_points() const noexcept
{
    return end_.column;
}

std::size_t GridLine::rows() const noexcept
{
    return grid_rows(end_.cell, columns_);
}

std::size_t GridLine::checkpoint_count() const noexcept
{
    return checkpoints_.size();
}

// state の位置の文字が始まる升（送りを含む）。末尾なら行の終わりの升。
std::size_t GridLine::start_of(const GridCheckpoint &state) const noexcept
{
    if (state.byte >= text_.size())
    {
        return state.cell;
    }
    auto copy = state;
    return step(text_, copy, columns_);
}

// code point の位置 column の直前の累積。
GridCheckpoint GridLine::at_column(std::size_t column) const
{
    column = std::min(column, end_.column);
    const auto found = std::ranges::upper_bound(checkpoints_, column, {}, &GridCheckpoint::column);
    if (found == checkpoints_.begin())
    {
        return checkpoints_.empty() ? end_ : GridCheckpoint{};
    }
    auto state = *std::prev(found);
    if (state.plain)
    {
        jump(state, column - state.column);
        return state;
    }
    while (state.column < column && state.byte < text_.size())
    {
        static_cast<void>(step(text_, state, columns_));
    }
    return state;
}

// 始まる升が cell 以上になる最初の文字の直前の累積。無ければ行の終わり。
GridCheckpoint GridLine::at_cell(std::size_t cell) const
{
    const auto found = std::ranges::upper_bound(checkpoints_, cell, {}, &GridCheckpoint::cell);
    if (found == checkpoints_.begin())
    {
        return checkpoints_.empty() ? end_ : GridCheckpoint{};
    }
    auto state = *std::prev(found);
    const auto limit = found == checkpoints_.end() ? end_.column : found->column;
    if (state.plain && cell - state.cell < limit - state.column)
    {
        jump(state, cell - state.cell);
        return state;
    }
    while (state.byte < text_.size())
    {
        auto next = state;
        if (step(text_, next, columns_) >= cell)
        {
            return state;
        }
        state = next;
    }
    return state;
}

GridPoint GridLine::point_of(std::size_t column, CaretAffinity affinity) const
{
    column = std::min(column, end_.column);
    bool at_end = column == end_.column;
    std::size_t cell = 0;
    if (affinity == CaretAffinity::trailing && column > 0)
    {
        auto before = at_column(column - 1);
        static_cast<void>(step(text_, before, columns_));
        cell = before.cell;
        at_end = true;
    }
    else
    {
        cell = start_of(at_column(column));
    }
    if (at_end && cell > 0 && cell % columns_ == 0)
    {
        return GridPoint{(cell / columns_) - 1, columns_};
    }
    return GridPoint{cell / columns_, cell % columns_};
}

DisplayCaret GridLine::hit(std::size_t row, float cell) const
{
    const auto low = row * columns_;
    const auto high = low + columns_;
    const float target =
        static_cast<float>(low) + std::clamp(cell, 0.0F, static_cast<float>(columns_));
    auto state = at_cell(low);
    while (state.byte < text_.size())
    {
        auto next = state;
        const auto start = step(text_, next, columns_);
        if (start >= high)
        {
            return DisplayCaret{Column{state.column + 1}, CaretAffinity::trailing};
        }
        const float middle = (static_cast<float>(start) + static_cast<float>(next.cell)) * half;
        if (target < middle)
        {
            return DisplayCaret{Column{state.column + 1}, CaretAffinity::leading};
        }
        state = next;
    }
    return DisplayCaret{Column{state.column + 1}, CaretAffinity::leading};
}

GridRowSpan GridLine::row_span(std::size_t row) const
{
    const auto low = row * columns_;
    const auto first = at_cell(low);
    const auto last = at_cell(low + columns_);
    const auto start = start_of(first);
    const auto lead = first.column < last.column ? std::min(start - low, columns_) : 0;
    return GridRowSpan{first.byte,   last.byte, first.column, last.column,
                       lead,         first.virtual_column};
}

std::size_t grid_rows(std::size_t cells, std::size_t columns) noexcept
{
    columns = std::max<std::size_t>(columns, 1);
    return std::max<std::size_t>((cells + columns - 1) / columns, 1);
}

std::size_t grid_columns(std::int32_t width, float cell) noexcept
{
    if (width <= 0 || !(cell > 0.0F))
    {
        return 1;
    }
    return std::max<std::size_t>(
        static_cast<std::size_t>(std::floor(static_cast<float>(width) / cell)), 1);
}
} // namespace nenenib::core
