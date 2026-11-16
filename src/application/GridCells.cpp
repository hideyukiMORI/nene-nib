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

// 桁と升を同じだけ（送りなしで）足す。plain の部分は 1 バイト＝1 桁＝1 升＝1 コードポイント。
void added(core::VirtualColumnCursor &state, std::size_t cells, std::size_t code_points) noexcept
{
    state.column += cells;
    state.cell += cells;
    state.code_point += code_points;
}

[[nodiscard]] core::GridPoint point_at(std::size_t cell, std::size_t columns) noexcept
{
    return core::GridPoint{cell / columns, cell % columns};
}

// 1 字が cells 升の字を count 字、升 start から grid_placed_cell と同じ規則で置いた終わりの升
// （#1711・一様な塊の閉じた式）。2 升以上で columns に収まる字だけが画面行の右端で送られる。
[[nodiscard]] std::size_t uniform_end(std::size_t start, std::size_t count, std::size_t cells,
                                      std::size_t columns) noexcept
{
    const auto room = columns - (start % columns);
    const auto first = room / cells;
    if (cells < 2 || cells > columns || count <= first)
    {
        return start + (count * cells);
    }
    const auto per_row = columns / cells;
    const auto later = count - first - 1;
    return start + room + ((later / per_row) * columns) + ((later % per_row) * cells) + cells;
}

// 同じ置き方で、升 target を占める（送りの空きなら送られた）字の序数（target が start より前なら
// 0）。呼ぶ側は target が終わりの升より前のときだけ読む。
[[nodiscard]] std::size_t uniform_ordinal(std::size_t start, std::size_t cells, std::size_t columns,
                                          std::size_t target) noexcept
{
    const auto offset = target - std::min(target, start);
    const auto room = columns - (start % columns);
    const auto first = room / cells;
    if (cells < 2 || cells > columns || offset < first * cells)
    {
        return offset / cells;
    }
    if (offset < room)
    {
        return first;
    }
    const auto per_row = columns / cells;
    const auto inside = offset - room;
    return first + ((inside / columns) * per_row) + std::min((inside % columns) / cells, per_row);
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

// 行 line の覚えを使う。違う行なら、行頭の累積だけを覚え直す。
void GridCells::remember(core::LineNumber line)
{
    if (remembered_line_ != line.value || starts_.empty())
    {
        remembered_line_ = line.value;
        starts_.assign(1, zero_cursor());
    }
}

// 行の first から数えて ordinal 番目の塊を、その塊の終わりまで進めたなら、次の塊の始まりを覚える。
void GridCells::noted(std::size_t ordinal, std::size_t chunk, core::OffsetRange range,
                      const core::VirtualColumnCursor &state)
{
    const auto &part = index_.chunks()[chunk];
    if (starts_.size() == ordinal + 1 && range.end.value == part.start + part.bytes)
    {
        starts_.push_back(state);
    }
}

std::optional<std::size_t> GridCells::reached(core::OffsetRange range, std::size_t chunk,
                                              core::VirtualColumnCursor &state, std::size_t target)
{
    const auto &part = index_.chunks()[chunk];
    const auto length = range.end.value - range.begin.value;
    if (part.plain)
    {
        if (target < state.cell + length)
        {
            return range.begin.value + (target - state.cell);
        }
        added(state, length, length);
        return std::nullopt;
    }
    if (part.uniform_bytes != 0)
    {
        return uniform_reached(range, part, state, target);
    }
    // 送りの対象の無い塊（Tab だけ・幅 1 の非 ASCII など）の全体は、桁の差だけ升も進む。終わりの
    // 桁は索引の要約から足す（塊を読まない）。target がこの塊に掛かるときだけ歩く。
    const bool whole = range.begin.value == part.start && length == part.bytes;
    if (whole && !part.wide && !part.resets)
    {
        const auto end = core::chunk_end(part);
        const auto cells = end.column - part.column;
        if (target >= state.cell + cells)
        {
            added(state, cells, part.code_points);
            return std::nullopt;
        }
    }
    return walked(range, state, target);
}

std::optional<std::size_t> GridCells::uniform_reached(core::OffsetRange range,
                                                      const core::VirtualColumnChunk &part,
                                                      core::VirtualColumnCursor &state,
                                                      std::size_t target) const noexcept
{
    const std::size_t unit = part.uniform_bytes;
    const std::size_t cells = part.uniform_cells;
    const auto count = (range.end.value - range.begin.value) / unit;
    const auto end = uniform_end(state.cell, count, cells, columns_);
    if (target < end)
    {
        return range.begin.value + (uniform_ordinal(state.cell, cells, columns_, target) * unit);
    }
    state.column += count * cells;
    state.cell = end;
    state.code_point += count;
    return std::nullopt;
}

std::optional<std::size_t> GridCells::walked(core::OffsetRange range,
                                             core::VirtualColumnCursor &state, std::size_t target)
{
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
    const auto first = first_chunk(begin);
    remember(line);
    // stop の手前の文字を含む塊より前の、覚えた最後の始まりから歩く。
    const auto last = stop > begin ? first_chunk(stop - 1) : first;
    const auto skipped = std::min(starts_.size() - 1, last - first);
    auto state = starts_[skipped];
    for (auto chunk = first + skipped; chunk < chunks.size() && chunks[chunk].start < stop; ++chunk)
    {
        const auto range = part_of(chunk, begin, stop);
        if (range.begin != range.end)
        {
            static_cast<void>(reached(range, chunk, state, unreachable_cell));
        }
        noted(chunk - first, chunk, range, state);
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
    const auto first = first_chunk(begin);
    remember(line);
    // 升が target を越えない、覚えた最後の始まりから歩く（それより前の文字は target
    // より前で終わる）。
    const auto known =
        std::ranges::upper_bound(starts_, target, {}, &core::VirtualColumnCursor::cell);
    const auto skipped = static_cast<std::size_t>(std::distance(starts_.begin(), known)) - 1;
    auto state = starts_[skipped];
    for (auto chunk = first + skipped; chunk < chunks.size() && chunks[chunk].start < end; ++chunk)
    {
        const auto range = part_of(chunk, begin, end);
        const auto found =
            range.begin == range.end ? std::nullopt : reached(range, chunk, state, target);
        if (found.has_value())
        {
            return core::Offset{found.value()};
        }
        noted(chunk - first, chunk, range, state);
    }
    return core::Offset{end};
}

std::size_t GridCells::rows(core::LineNumber line)
{
    return core::grid_rows(cursor_at(line, text_.line_end(line)).cell, columns_);
}
} // namespace nenenib::application
