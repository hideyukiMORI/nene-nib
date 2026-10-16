#include "VisualTextView.hpp"

#include "ComposedLine.hpp"
#include "GridPoint.hpp"
#include "ScrollBounds.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace nenenib::application
{
namespace
{
[[nodiscard]] core::VisualPosition visual_first(const ScrollState &scroll) noexcept
{
    return core::VisualPosition{scroll.first_visible, scroll.first_row};
}

[[nodiscard]] std::size_t magnitude(std::int64_t amount) noexcept
{
    return amount < 0 ? static_cast<std::size_t>(-(amount + 1)) + 1
                      : static_cast<std::size_t>(amount);
}

[[nodiscard]] core::DisplayCaret shown_caret(const LineView &line, core::TextCaret caret,
                                             core::TextPosition composing)
{
    auto column = core::Column{
        core::display_position(
            line.display, caret.position.column.value > 0 ? caret.position.column.value - 1 : 0) +
        1};
    if (line.composed.has_value())
    {
        const auto &inserted = line.composed.value();
        if (caret.position == composing)
        {
            return core::DisplayCaret{inserted.cursor, core::CaretAffinity::leading};
        }
        if (column.value >= inserted.insertion_column.value)
        {
            column.value += inserted.inserted_columns;
        }
    }
    return core::DisplayCaret{column, caret.affinity};
}
} // namespace

VisualTextView::VisualTextView(const EditorState &state, TextLayoutPort &layouts,
                               core::TextViewGeometry geometry)
    : state_(state), layouts_(layouts), geometry_(geometry),
      caret_(state.text().position_of(state.selection().caret))
{
    if (!gridded())
    {
        return;
    }
    // 1 升の幅は ui の事実なので port から読む（ADR 0140・試作の D）。升の数は窓の幅に入る数。
    const auto cell = layouts_.cell(TextLayoutRequest{{}, state_.settings(), geometry_});
    if (!cell)
    {
        grid_failure_ = cell.error();
        return;
    }
    cell_width_ = cell.value().width;
    grid_columns_ =
        static_cast<std::size_t>(std::max(static_cast<float>(geometry_.width) / cell_width_, 1.0F));
}

LineView VisualTextView::line_of(const EditorState &state, core::LineNumber number)
{
    number.value = std::clamp<std::size_t>(number.value, 1, state.text().line_count());
    auto text = state.text().line_text(number);
    auto display = core::display_line(text);
    LineView result{number, std::move(text), std::move(display), core::no_selection_span(),
                    {},     std::nullopt};
    const auto &composition = state.composition();
    if (!composition.has_value() || state.command_input().has_value())
    {
        return result;
    }
    const auto caret = state.text().position_of(state.selection().caret);
    if (number == caret.line)
    {
        result.composed = composed_line(result.display, caret.column, composition.value());
    }
    return result;
}

LineView VisualTextView::line(core::LineNumber number) const
{
    return line_of(state_, number);
}

bool VisualTextView::gridded() const noexcept
{
    return state_.display() == core::DisplayForm::grid && geometry_.width > 0;
}

std::expected<GridCells, TextLayoutFailure> VisualTextView::grid() const
{
    if (grid_columns_ == 0)
    {
        return std::unexpected(grid_failure_.value_or(TextLayoutFailure::unavailable));
    }
    const auto index = state_.text().virtual_columns();
    if (!index)
    {
        return std::unexpected(TextLayoutFailure::unavailable);
    }
    return GridCells{index.value().get(), state_.text(), grid_columns_};
}

std::expected<std::size_t, TextLayoutFailure>
VisualTextView::grid_rows(core::LineNumber number) const
{
    auto cells = grid();
    if (!cells)
    {
        return std::unexpected(cells.error());
    }
    number.value = std::clamp<std::size_t>(number.value, 1, state_.text().line_count());
    return cells.value().rows(number);
}

std::expected<std::reference_wrapper<const core::VirtualColumnIndex>, TextLayoutFailure>
VisualTextView::grid_index() const
{
    if (!grid_.has_value())
    {
        return std::unexpected(grid_failure_.value_or(TextLayoutFailure::unavailable));
    }
    return std::cref(grid_.value());
}

// 行頭に立つ trailing のキャレットは前の画面行の右端に描く（文字組みの hit と同じ見え方）。
std::expected<core::GridPoint, TextLayoutFailure>
VisualTextView::grid_point(core::TextCaret caret) const
{
    auto cells = grid();
    if (!cells)
    {
        return std::unexpected(cells.error());
    }
    const auto &text = state_.text();
    auto point = cells.value().point_of(caret.position.line, text.offset_of(caret.position));
    if (caret.affinity == core::CaretAffinity::trailing && point.cell == 0 && point.row > 0)
    {
        point = core::GridPoint{point.row - 1, grid_columns_};
    }
    return point;
}

std::expected<core::TextLocation, TextLayoutFailure>
VisualTextView::grid_location(core::TextCaret caret) const
{
    const auto point = grid_point(caret);
    if (!point)
    {
        return std::unexpected(point.error());
    }
    return core::TextLocation{point.value().row,
                              static_cast<float>(point.value().cell) * cell_width_};
}

// x は最も近い升の境目へ丸める。右端（columns）を指したら、そこに立つ文字の前に trailing で置く。
std::expected<core::TextCaret, TextLayoutFailure>
VisualTextView::grid_caret_at(core::VisualPosition at, float x) const
{
    auto cells = grid();
    if (!cells)
    {
        return std::unexpected(cells.error());
    }
    const auto &text = state_.text();
    const core::LineNumber number{std::clamp<std::size_t>(at.line.value, 1, text.line_count())};
    const auto rows = cells.value().rows(number);
    const auto nearest = std::clamp((std::max(x, 0.0F) / cell_width_) + 0.5F, 0.0F,
                                    static_cast<float>(grid_columns_));
    const core::GridPoint point{std::min(at.row, rows - 1), static_cast<std::size_t>(nearest)};
    const auto offset = cells.value().byte_of(number, point);
    const auto affinity = point.cell == grid_columns_ && offset != text.line_end(number)
                              ? core::CaretAffinity::trailing
                              : core::CaretAffinity::leading;
    return core::TextCaret{text.position_of(offset), affinity};
}

TextLayoutRequest VisualTextView::request(const LineView &line) const
{
    return TextLayoutRequest{presented_text(line), state_.settings(), geometry_};
}

std::expected<std::size_t, TextLayoutFailure> VisualTextView::rows(core::LineNumber number) const
{
    if (geometry_.width <= 0)
    {
        return 1;
    }
    if (gridded())
    {
        return grid_rows(number);
    }
    const auto shown = line(number);
    return layouts_.rows(request(shown));
}

std::expected<core::TextLocation, TextLayoutFailure>
VisualTextView::location(core::TextCaret caret) const
{
    if (geometry_.width <= 0)
    {
        return core::TextLocation{0, static_cast<float>(caret.position.column.value - 1)};
    }
    if (gridded())
    {
        return grid_location(caret);
    }
    const auto shown = line(caret.position.line);
    return layouts_.locate(request(shown), shown_caret(shown, caret, caret_));
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::position(core::TextCaret caret) const
{
    const auto located = location(caret);
    if (!located)
    {
        return std::unexpected(located.error());
    }
    return core::VisualPosition{caret.position.line, located.value().row};
}

std::expected<core::TextCaret, TextLayoutFailure> VisualTextView::caret_at(core::VisualPosition at,
                                                                           float x) const
{
    if (geometry_.width <= 0)
    {
        const core::TextPosition desired{
            at.line, core::Column{static_cast<std::size_t>(std::max(x, 0.0F)) + 1}};
        return core::TextCaret{state_.text().position_of(state_.text().offset_of(desired))};
    }
    if (gridded())
    {
        return grid_caret_at(at, x);
    }
    const auto shown = line(at.line);
    const auto hit = layouts_.hit(request(shown), core::TextLocation{at.row, x});
    if (!hit)
    {
        return std::unexpected(hit.error());
    }
    const auto column = shown.composed.has_value()
                            ? uncomposed_column(shown.composed.value(), hit.value().column)
                            : hit.value().column;
    return core::TextCaret{
        core::TextPosition{shown.number,
                           core::Column{core::source_column(shown.display, column.value - 1) + 1}},
        hit.value().affinity};
}

std::expected<core::VisualPosition, TextLayoutFailure> VisualTextView::last() const
{
    const core::LineNumber number{state_.text().line_count()};
    const auto count = rows(number);
    if (!count)
    {
        return std::unexpected(count.error());
    }
    return core::VisualPosition{number, count.value() - 1};
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::forward(core::VisualPosition from, std::size_t amount) const
{
    while (true)
    {
        const auto count = rows(from.line);
        if (!count)
        {
            return std::unexpected(count.error());
        }
        from.row = std::min(from.row, count.value() - 1);
        const auto available = count.value() - 1 - from.row;
        if (amount <= available)
        {
            return core::VisualPosition{from.line, from.row + amount};
        }
        if (from.line.value == state_.text().line_count())
        {
            return core::VisualPosition{from.line, count.value() - 1};
        }
        amount -= available + 1;
        ++from.line.value;
        from.row = 0;
    }
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::backward(core::VisualPosition from, std::size_t amount) const
{
    while (true)
    {
        const auto count = rows(from.line);
        if (!count)
        {
            return std::unexpected(count.error());
        }
        from.row = std::min(from.row, count.value() - 1);
        if (amount <= from.row)
        {
            return core::VisualPosition{from.line, from.row - amount};
        }
        if (from.line.value == 1)
        {
            return core::VisualPosition{core::LineNumber{1}, 0};
        }
        amount -= from.row + 1;
        --from.line.value;
        from.row = std::numeric_limits<std::size_t>::max();
    }
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::shifted(core::VisualPosition from, std::int64_t amount) const
{
    from.line.value = std::clamp<std::size_t>(from.line.value, 1, state_.text().line_count());
    const auto steps = magnitude(amount);
    // 制御文字の表示は最大6文字/byte。巨大な回数で全文書を歩かない。
    const auto &composed = state_.composition();
    const auto composition = composed.has_value() ? composed.value().utf8.size() : 0;
    const auto maximum = state_.text().size_bytes() * 6 + state_.text().line_count() + composition;
    if (steps >= maximum)
    {
        return amount < 0
                   ? std::expected<core::VisualPosition, TextLayoutFailure>{core::VisualPosition{
                         core::LineNumber{1}, 0}}
                   : last();
    }
    return amount < 0 ? backward(from, steps) : forward(from, steps);
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::constrained(core::VisualPosition first, core::ScrollExtent extent) const
{
    const auto normalized = shifted(first, 0);
    if (!normalized)
    {
        return std::unexpected(normalized.error());
    }
    switch (extent)
    {
    case core::ScrollExtent::last_line:
        return normalized;
    case core::ScrollExtent::filled_viewport:
        break;
    }
    // 残る論理行だけで画面が埋まるなら、文書末尾の文字組みは要らない。
    if (state_.text().line_count() - normalized.value().line.value >=
        std::max<std::size_t>(state_.scroll().visible_lines, 1) - 1)
    {
        return normalized;
    }
    const auto end = last();
    if (!end)
    {
        return std::unexpected(end.error());
    }
    const auto highest =
        backward(end.value(), std::max<std::size_t>(state_.scroll().visible_lines, 1) - 1);
    if (!highest)
    {
        return std::unexpected(highest.error());
    }
    return highest.value() < normalized.value() ? highest : normalized;
}

std::expected<std::size_t, TextLayoutFailure> VisualTextView::distance(core::VisualPosition first,
                                                                       core::VisualPosition last,
                                                                       std::size_t limit) const
{
    if (last.line.value - first.line.value >= limit)
    {
        return limit;
    }
    std::size_t result = 0;
    while (first.line < last.line)
    {
        const auto count = rows(first.line);
        if (!count)
        {
            return std::unexpected(count.error());
        }
        const auto step = count.value() - std::min(first.row, count.value() - 1);
        if (step >= limit - result)
        {
            return limit;
        }
        result += step;
        ++first.line.value;
        first.row = 0;
    }
    return result + std::min(last.row - first.row, limit - result);
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::followed_first(core::VisualPosition caret, core::ScrollFollow follow) const
{
    const auto normalized = shifted(visual_first(state_.scroll()), 0);
    if (!normalized)
    {
        return std::unexpected(normalized.error());
    }
    const auto first = normalized.value();
    const auto height = std::max<std::size_t>(state_.scroll().visible_lines, 1);
    const auto limit = height * 2 + 1;
    const bool above = caret < first;
    const auto delta = above ? distance(caret, first, limit) : distance(first, caret, limit);
    if (!delta)
    {
        return std::unexpected(delta.error());
    }
    const core::LineNumber virtual_first{limit + 1};
    const core::LineNumber virtual_caret{above ? virtual_first.value - delta.value()
                                               : virtual_first.value + delta.value()};
    const auto target = core::first_visible_for_caret(virtual_first, virtual_caret, height, follow);
    if (target == virtual_first)
    {
        return first;
    }
    return backward(caret, virtual_caret.value - target.value);
}

std::expected<ScrollState, TextLayoutFailure>
VisualTextView::followed(core::TextCaret caret, core::ScrollFollow follow,
                         core::ScrollExtent extent) const
{
    const auto point = location(caret);
    if (!point)
    {
        return std::unexpected(point.error());
    }
    const auto requested =
        followed_first(core::VisualPosition{caret.position.line, point.value().row}, follow);
    const auto filled =
        constrained(visual_first(state_.scroll()), core::ScrollExtent::filled_viewport);
    if (!requested || !filled)
    {
        return std::unexpected(!requested ? requested.error() : filled.error());
    }
    const auto selected = filled.value() < visual_first(state_.scroll())
                              ? extent
                              : core::ScrollExtent::filled_viewport;
    const auto first = constrained(requested.value(), selected);
    if (!first)
    {
        return std::unexpected(first.error());
    }
    auto result = state_.scroll();
    result.first_visible = first.value().line;
    result.first_row = first.value().row;
    result.horizontal = 0;
    // 格子は折り返すので横の送りは無い（ADR 0140 の決定 7）。
    if (state_.settings().wrapping == core::LineWrapping::no_wrap && geometry_.width > 0 &&
        !gridded())
    {
        const auto width = static_cast<float>(std::max(geometry_.width - 3, 1));
        const auto x = std::max(point.value().x, 0.0F);
        result.horizontal = std::clamp(state_.scroll().horizontal, std::max(x - width, 0.0F), x);
    }
    return result;
}
} // namespace nenenib::application
