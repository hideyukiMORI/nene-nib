#include "GridLineRows.hpp"

#include "DisplayLine.hpp"
#include "Grid.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

namespace nenenib::application
{
namespace
{
constexpr char32_t tab_character = U'\t';
// 見えている最後の画面行の次の画面行の始まりから、そこで始まる Tab（この画面行に升が掛かる）を
// 読むのに足りる長さ。
constexpr std::size_t peek_bytes = 8;
constexpr std::string_view space = " ";

// text の byte から count 個の code point を進めた位置。
[[nodiscard]] std::size_t skipped(std::string_view text, std::size_t byte, std::size_t count)
{
    for (std::size_t index = 0; index < count && byte < text.size(); ++index)
    {
        byte = core::next_code_point(text, core::Offset{byte}).value;
    }
    return byte;
}

[[nodiscard]] std::size_t piece_offset(const std::array<std::size_t, 3> &piece) noexcept
{
    return piece.at(0);
}

// 変換中の文字列の code point ごとの始まりの升（origin から・末尾は升の和）。
[[nodiscard]] std::vector<std::size_t> composition_starts(std::string_view utf8, std::size_t origin)
{
    std::vector<std::size_t> starts{origin};
    for (std::size_t byte = 0; byte < utf8.size();)
    {
        starts.push_back(starts.back() +
                         core::column_advance(core::code_point_at(utf8, core::Offset{byte}), 0));
        byte = core::next_code_point(utf8, core::Offset{byte}).value;
    }
    return starts;
}

// 差し込んだ升（from から cells 升）の後ろへ範囲をずらす。始まりが from 以降ならずらし、終わりが
// from より後ろならずらす（from で終わる範囲は差し込みの手前のまま）。
[[nodiscard]] GridCellSpan shifted_span(GridCellSpan span, std::size_t from,
                                        std::size_t cells) noexcept
{
    return GridCellSpan{span.begin >= from ? span.begin + cells : span.begin,
                        span.end > from ? span.end + cells : span.end};
}

// 構文の色は差し込んだ升に掛けない（#839）。差し込みをまたぐ区間は手前と後ろの 2 つに割る。
[[nodiscard]] std::vector<GridColorSpan> split_colors(const std::vector<GridColorSpan> &colors,
                                                      std::size_t from, std::size_t cells)
{
    std::vector<GridColorSpan> result;
    result.reserve(colors.size() + 1);
    for (const auto &color : colors)
    {
        const auto span = color.cells;
        if (span.begin < from && span.end > from)
        {
            result.push_back(GridColorSpan{GridCellSpan{span.begin, from}, color.role});
            result.push_back(
                GridColorSpan{GridCellSpan{from + cells, span.end + cells}, color.role});
            continue;
        }
        result.push_back(GridColorSpan{shifted_span(span, from, cells), color.role});
    }
    return result;
}

void shift_spans(GridRowView &row, std::size_t from, std::size_t cells)
{
    const auto moved = [from, cells](GridCellSpan span) { return shifted_span(span, from, cells); };
    row.selection = row.selection.transform(moved);
    row.current_match = row.current_match.transform(moved);
    std::ranges::transform(row.matches, row.matches.begin(), moved);
    std::ranges::transform(row.replaced, row.replaced.begin(), moved);
    row.colors = split_colors(row.colors, from, cells);
}

// 差し込む位置: キャレットの升で始まる最初の code point の後ろの、幅 0 の code point（前の文字の
// 結合文字）を越えた所（ADR 0053）。行頭の幅 0 は前の文字が無いので越えない。
[[nodiscard]] std::vector<std::size_t>::iterator insertion_of(std::vector<std::size_t> &starts,
                                                              std::size_t cell)
{
    const auto last = std::prev(starts.end());
    auto first = std::ranges::lower_bound(starts.begin(), last, cell);
    while (first != starts.begin() && first != last && *std::next(first) == *first)
    {
        ++first;
    }
    return first;
}

[[nodiscard]] GridRowView empty_row(const GridLineSource &source, std::size_t row)
{
    return GridRowView{source.line,
                       row,
                       source.screen_row + static_cast<std::int64_t>(row - source.first_row),
                       {},
                       {},
                       std::nullopt,
                       false,
                       {},
                       std::nullopt,
                       {},
                       std::nullopt,
                       std::nullopt,
                       {}};
}
} // namespace

GridLineRows::GridLineRows(const GridLineSource &source)
    : source_(source), columns_(source.cells.columns()), last_row_(source.first_row)
{
    for (auto row = source.first_row; row < source.end_row; ++row)
    {
        rows_.push_back(empty_row(source, row));
        pieces_.emplace_back();
        ends_.push_back(0);
    }
    const auto &state = source.state;
    caret_shown_ = state.text().position_of(state.selection().caret).line == source.line &&
                   shown(source.caret.row);
    walk();
    finish();
    mark();
}

void GridLineRows::append_to(std::vector<GridRowView> &rows)
{
    std::ranges::move(rows_, std::back_inserter(rows));
    rows_.clear();
}

bool GridLineRows::shown(std::size_t row) const noexcept
{
    return row >= source_.first_row && row < source_.end_row;
}

// 見えている最初の画面行の左端の文字から、見えている最後の画面行の右端を越えるまで歩く。
void GridLineRows::walk()
{
    const auto &text = source_.state.text();
    const auto end = text.line_end(source_.line);
    auto &cells = source_.cells;
    const auto begin = source_.visible.begin;
    const auto next = source_.visible.end;
    const core::Offset stop{std::max(std::min(next.value + peek_bytes, end.value), begin.value)};
    const auto cursor = cells.cursor_at(source_.line, begin);
    column_ = cursor.column;
    cell_ = cursor.cell;
    const auto bytes = text.text_range(begin, stop);
    const auto display = core::display_line(bytes);
    std::size_t at = 0;
    std::size_t drawn = 0;
    for (std::size_t index = 0; at < bytes.size() && !stopped_; ++index)
    {
        const auto following =
            skipped(display.text, drawn, display.starts.at(index + 1) - display.starts.at(index));
        step(core::Offset{begin.value + at}, core::code_point_at(bytes, core::Offset{at}),
             std::string_view(display.text).substr(drawn, following - drawn));
        drawn = following;
        at = core::next_code_point(bytes, core::Offset{at}).value;
    }
}

// 1 文字を升に置く。送りの空き（grid_placed_cell が次の画面行へ送った分）は空白で埋める。
void GridLineRows::step(core::Offset at, char32_t value, std::string_view drawn)
{
    const auto advance = core::column_advance(value, column_);
    const auto start =
        value == tab_character ? cell_ : core::grid_placed_cell(cell_, advance, columns_);
    if (advance > 0 && start >= source_.end_row * columns_)
    {
        stopped_ = true;
        return;
    }
    blank(cell_, start, std::nullopt);
    if (value == tab_character)
    {
        blank(start, start + advance, at);
    }
    else if (advance == 0)
    {
        attach(at, drawn);
    }
    else
    {
        put(at, start, advance, drawn);
    }
    column_ += advance;
    cell_ = start + advance;
}

// 置き換えの表示（`^M` など・ADR 0040）は 1 升ずつの ASCII、ほかは 1 文字が cells 升。
void GridLineRows::put(core::Offset at, std::size_t start, std::size_t cells,
                       std::string_view drawn)
{
    const auto row = start / columns_;
    last_row_ = row;
    if (!shown(row))
    {
        return;
    }
    const auto index = row - source_.first_row;
    const auto local = ends_.at(index);
    if (core::code_point_count(drawn) <= 1)
    {
        emit(row, cells, drawn, at);
        return;
    }
    for (std::size_t byte = 0; byte < drawn.size();)
    {
        const auto next = core::next_code_point(drawn, core::Offset{byte}).value;
        emit(row, 1, drawn.substr(byte, next - byte), at);
        byte = next;
    }
    rows_.at(index).replaced.push_back(GridCellSpan{local, ends_.at(index)});
}

void GridLineRows::blank(std::size_t from, std::size_t to, std::optional<core::Offset> owner)
{
    for (auto cell = from; cell < to; ++cell)
    {
        if (shown(cell / columns_))
        {
            emit(cell / columns_, 1, space, owner);
        }
    }
}

// 幅 0 の文字は直前の文字の升に重ねる（升を進めない）。
void GridLineRows::attach(core::Offset at, std::string_view drawn)
{
    if (shown(last_row_))
    {
        emit(last_row_, 0, drawn, at);
    }
}

void GridLineRows::emit(std::size_t row, std::size_t cells, std::string_view drawn,
                        std::optional<core::Offset> owner)
{
    const auto index = row - source_.first_row;
    auto &view = rows_.at(index);
    const auto local = ends_.at(index);
    view.text.append(drawn);
    view.starts.push_back(local);
    ends_.at(index) = local + cells;
    if (!owner.has_value())
    {
        return;
    }
    auto &pieces = pieces_.at(index);
    if (!pieces.empty() && piece_offset(pieces.back()) == owner.value().value)
    {
        pieces.back().at(2) = local + cells;
        return;
    }
    pieces.push_back(Piece{owner.value().value, local, local + cells});
}

void GridLineRows::finish()
{
    for (std::size_t index = 0; index < rows_.size(); ++index)
    {
        rows_.at(index).starts.push_back(ends_.at(index));
    }
}

std::optional<GridCellSpan> GridLineRows::span_of(std::size_t index,
                                                  const core::OffsetRange &range) const
{
    std::optional<std::size_t> begin;
    std::size_t end = 0;
    for (const auto &piece : pieces_.at(index))
    {
        if (piece_offset(piece) < range.begin.value || piece_offset(piece) >= range.end.value)
        {
            continue;
        }
        begin = begin.value_or(piece.at(1));
        end = std::max(end, piece.at(2));
    }
    if (!begin.has_value() || begin.value() >= end)
    {
        return std::nullopt;
    }
    return GridCellSpan{begin.value(), end};
}

void GridLineRows::mark()
{
    // 歩き終えた行の最後の画面行（行の終わりが右端にちょうど達したらその画面行）。
    const auto last = core::grid_rows(cell_, columns_) - 1;
    for (std::size_t index = 0; index < rows_.size(); ++index)
    {
        mark_row(index, last);
    }
    mark_caret();
    compose();
}

void GridLineRows::mark_row(std::size_t index, std::size_t last)
{
    const auto &marks = source_.marks;
    const auto end = source_.state.text().line_end(source_.line);
    auto &row = rows_.at(index);
    row.selection = span_of(index, marks.selection);
    row.newline_selected = !stopped_ && row.row == last &&
                           marks.selection.begin.value <= end.value &&
                           marks.selection.end.value > end.value;
    for (const auto &match : marks.matches)
    {
        const auto span = span_of(index, match);
        if (span.has_value())
        {
            row.matches.push_back(span.value());
        }
    }
    row.current_match =
        marks.current.has_value() ? span_of(index, marks.current.value()) : std::nullopt;
    for (const auto &color : marks.colors)
    {
        const auto span = span_of(index, color.range);
        if (span.has_value())
        {
            row.colors.push_back(GridColorSpan{span.value(), color.role});
        }
    }
}

// キャレットの升は VisualTextView と同じ升（source_.caret）。ブロックはそこに立つ文字の升の数。
void GridLineRows::mark_caret()
{
    if (!caret_shown_)
    {
        return;
    }
    const auto point = source_.caret;
    const auto index = point.row - source_.first_row;
    const auto at = source_.state.selection().caret.value;
    std::size_t width = 1;
    for (const auto &piece : pieces_.at(index))
    {
        if (piece_offset(piece) == at && piece.at(1) == point.cell && piece.at(2) > piece.at(1))
        {
            width = piece.at(2) - piece.at(1);
        }
    }
    rows_.at(index).caret = GridCellSpan{point.cell, point.cell + width};
}

// 変換中の文字列はキャレットの升に差し込み、後ろの升（文字・選択・当たり・置き換え）を同じ升数
// だけずらす（本文の外のまま・ADR 0014）。
void GridLineRows::compose()
{
    const auto &state = source_.state;
    const auto &composed = state.composition();
    if (!composed.has_value() || state.command_input().has_value() || !caret_shown_)
    {
        return;
    }
    const auto &composition = composed.value();
    const auto point = source_.caret;
    auto &row = rows_.at(point.row - source_.first_row);
    const auto inserted = composition_starts(composition.utf8, point.cell);
    const auto cells = inserted.back() - point.cell;
    const auto first = insertion_of(row.starts, point.cell);
    const auto position = static_cast<std::size_t>(std::distance(row.starts.begin(), first));
    for (auto shifted = first; shifted != row.starts.end(); ++shifted)
    {
        *shifted += cells;
    }
    row.starts.insert(first, inserted.begin(), std::prev(inserted.end()));
    shift_spans(row, point.cell, cells);
    row.text.insert(skipped(row.text, 0, position), composition.utf8);
    row.composed_at = position;
    const auto cursor = inserted.at(core::code_point_count(
        std::string_view(composition.utf8)
            .substr(0, std::min(composition.cursor.value, composition.utf8.size()))));
    row.caret = GridCellSpan{cursor, cursor + 1};
}
} // namespace nenenib::application
