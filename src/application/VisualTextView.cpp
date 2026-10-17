#include "VisualTextView.hpp"

#include "ComposedLine.hpp"
#include "GiantLine.hpp"
#include "GiantLineWindow.hpp"
#include "GridPoint.hpp"
#include "ScrollBounds.hpp"
#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <utility>
#include <variant>

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

// 巨大な行の窓では、本文の桁を窓の中の桁に写す（ADR 0139）。窓の外は窓の端に畳む。
[[nodiscard]] core::DisplayCaret shown_caret(const LineView &line, core::TextCaret caret,
                                             core::TextPosition composing)
{
    const auto source = caret.position.column.value > 0 ? caret.position.column.value - 1 : 0;
    auto column = core::Column{
        core::display_position(line.display, source - std::min(source, line.window.code_point)) +
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

// キャレットの行の窓の始まりの仮想桁（キャレットが巨大な行に無ければ 0・ADR 0139 の決定 5）。
[[nodiscard]] std::size_t origin_of(const EditorState &state, core::TextPosition caret)
{
    const auto &text = state.text();
    const auto *index = giant_line_index(text, caret.line);
    if (index == nullptr)
    {
        return 0;
    }
    return giant_window_at(*index, text, caret.line, state.selection().caret).first.column;
}
// 見えている行数に足す保持の余り（画面の上下に半分かかる行と、キャレットの行）。
constexpr std::size_t shown_margin = 4;
} // namespace

VisualTextView::VisualTextView(const EditorState &state, TextLayoutPort &layouts,
                               core::TextViewGeometry geometry)
    : state_(state), layouts_(layouts), geometry_(geometry),
      caret_(state.text().position_of(state.selection().caret)), advance_(window_advance(geometry))
{
    if (!gridded())
    {
        origin_ = origin_of(state_, caret_);
        return;
    }
    // 1 升の幅は ui の事実なので port から読む（ADR 0140・試作の D）。升の数は窓の幅に入る数。
    const auto cell =
        layouts_.cell(TextLayoutRequest{{}, state_.settings(), geometry_, core::DisplayForm::grid});
    if (!cell)
    {
        grid_failure_ = cell.error();
        return;
    }
    cell_width_ = cell.value().width;
    grid_columns_ =
        static_cast<std::size_t>(std::max(static_cast<float>(geometry_.width) / cell_width_, 1.0F));
}

LineView VisualTextView::line(core::LineNumber number) const
{
    return line_at(number, std::nullopt);
}

LineView VisualTextView::line_at(core::LineNumber number, std::optional<core::Offset> anchor) const
{
    number.value = std::clamp<std::size_t>(number.value, 1, state_.text().line_count());
    if (const auto *index = giant_line_index(state_.text(), number); index != nullptr)
    {
        return giant_line(number, *index, anchor);
    }
    auto text = state_.text().line_text(number);
    auto display = core::display_line(text);
    LineView result{number, std::move(text), std::move(display), core::no_selection_span(),
                    {},     std::nullopt};
    compose(result);
    result.left = offset_of(result) - state_.scroll().horizontal;
    return result;
}

// 窓の中の本文だけを text_range で読む（決定 3）。錨はキャレットか、横の送りの左端の仮想桁。
LineView VisualTextView::giant_line(core::LineNumber number, const core::VirtualColumnIndex &index,
                                    std::optional<core::Offset> anchor) const
{
    const auto &text = state_.text();
    if (!anchor.has_value())
    {
        const auto left =
            static_cast<std::size_t>(std::max(state_.scroll().horizontal, 0.0F) / advance_);
        anchor = number == caret_.line ? state_.selection().caret
                                       : index.byte_at_virtual_column(text, number, origin_ + left);
    }
    auto [window, range] = giant_window_at(index, text, number, anchor.value());
    window.x = window_x(window.column, origin_, advance_);
    auto shown = text.text_range(range.begin, range.end);
    auto display = core::display_line(shown);
    LineView result{number, std::move(shown), std::move(display), core::no_selection_span(),
                    {},     std::nullopt};
    result.window = window;
    compose(result);
    result.left = offset_of(result) - state_.scroll().horizontal;
    return result;
}

void VisualTextView::compose(LineView &result) const
{
    const auto &composition = state_.composition();
    if (result.number != caret_.line || !composition.has_value() ||
        state_.command_input().has_value())
    {
        return;
    }
    const auto shift = std::min(result.window.code_point, caret_.column.value - 1);
    result.composed = composed_line(result.display, core::Column{caret_.column.value - shift},
                                    composition.value());
}

// 折り返す設定の普通の行は送らない（原点は欄の左端・x は送りを足した値）。巨大な行は窓の x、
// 折り返さない設定の普通の行は行頭（origin の窓から見た行頭の x）。
float VisualTextView::offset_of(const LineView &line) const noexcept
{
    if (line.window.active)
    {
        return line.window.x;
    }
    if (state_.settings().wrapping == core::LineWrapping::wrap || geometry_.width <= 0)
    {
        return state_.scroll().horizontal;
    }
    return -static_cast<float>(origin_) * advance_;
}

bool VisualTextView::gridded() const noexcept
{
    return state_.display() == core::DisplayForm::grid && geometry_.width > 0;
}

std::expected<GridCells, TextLayoutFailure> VisualTextView::grid_cells() const
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
    auto cells = grid_cells();
    if (!cells)
    {
        return std::unexpected(cells.error());
    }
    number.value = std::clamp<std::size_t>(number.value, 1, state_.text().line_count());
    return cells.value().rows(number);
}

// 行頭に立つ trailing のキャレットは前の画面行の右端に描く（文字組みの hit と同じ見え方）。
core::GridPoint VisualTextView::grid_point(GridCells &cells, core::TextCaret caret) const
{
    auto point = cells.point_of(caret.position.line, state_.text().offset_of(caret.position));
    if (caret.affinity == core::CaretAffinity::trailing && point.cell == 0 && point.row > 0)
    {
        point = core::GridPoint{point.row - 1, grid_columns_};
    }
    return point;
}

std::expected<core::GridPoint, TextLayoutFailure>
VisualTextView::grid_point(core::TextCaret caret) const
{
    auto cells = grid_cells();
    if (!cells)
    {
        return std::unexpected(cells.error());
    }
    return grid_point(cells.value(), caret);
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
    auto cells = grid_cells();
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

VisualTextView::SharedLine VisualTextView::shown(core::LineNumber number)
{
    number.value = std::clamp<std::size_t>(number.value, 1, state_.text().line_count());
    const auto found = std::ranges::find_if(shown_, [number](const SharedLine &value)
                                            { return value->number == number; });
    if (found != shown_.end())
    {
        return *found;
    }
    // 歩きが上限を超えて戻ってきたら作り直す（正しさは変わらない）。
    const auto limit = std::max<std::size_t>(state_.scroll().visible_lines, 1) + shown_margin;
    if (shown_.size() >= limit)
    {
        shown_.erase(shown_.begin());
    }
    shown_.push_back(std::make_shared<const LineView>(line(number)));
    return shown_.back();
}

TextLayoutRequest VisualTextView::request(const LineView &line) const
{
    return TextLayoutRequest{
        presented_text(line), state_.settings(), geometry_, core::DisplayForm::text, {},
        line.window};
}

// 巨大な行は折り返さないので 1 画面行（ADR 0139 の決定 2）。行の全体を読まない。
std::expected<std::size_t, TextLayoutFailure> VisualTextView::rows(core::LineNumber number)
{
    if (geometry_.width <= 0)
    {
        return 1;
    }
    if (gridded())
    {
        return grid_rows(number);
    }
    number.value = std::clamp<std::size_t>(number.value, 1, state_.text().line_count());
    if (giant_line_index(state_.text(), number) != nullptr)
    {
        return 1;
    }
    const auto made = shown(number);
    return layouts_.rows(request(*made));
}

std::expected<core::TextLocation, TextLayoutFailure> VisualTextView::location(core::TextCaret caret)
{
    if (geometry_.width <= 0)
    {
        return core::TextLocation{0, static_cast<float>(caret.position.column.value - 1)};
    }
    if (gridded())
    {
        return grid_location(caret);
    }
    // 巨大な行は位置そのものを錨にした窓で求める（incsearch の preview も窓の中に来る）。
    const auto &text = state_.text();
    const auto *index = giant_line_index(text, caret.position.line);
    // 巨大な行の窓は位置ごとに錨が違うので保持を通さない（#529 の保持は既定の錨の行だけ）。
    const auto made =
        index == nullptr ? shown(caret.position.line)
                         : std::make_shared<const LineView>(line_at(
                               caret.position.line, giant_offset_of(*index, text, caret.position)));
    const auto &shown_line = *made;
    auto located = layouts_.locate(request(shown_line), shown_caret(shown_line, caret, caret_));
    if (located)
    {
        located.value().x += offset_of(shown_line);
    }
    return located;
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::position(core::TextCaret caret)
{
    const auto located = location(caret);
    if (!located)
    {
        return std::unexpected(located.error());
    }
    return core::VisualPosition{caret.position.line, located.value().row};
}

std::expected<core::TextCaret, TextLayoutFailure> VisualTextView::caret_at(core::VisualPosition at,
                                                                           float x)
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
    const auto made = shown(at.line);
    const auto &shown_line = *made;
    const auto local = x - offset_of(shown_line);
    if (const auto outside = outside_window(shown_line, local); outside.has_value())
    {
        return outside.value();
    }
    const auto hit = layouts_.hit(request(shown_line), core::TextLocation{at.row, local});
    if (!hit)
    {
        return std::unexpected(hit.error());
    }
    const auto column = shown_line.composed.has_value()
                            ? uncomposed_column(shown_line.composed.value(), hit.value().column)
                            : hit.value().column;
    return core::TextCaret{
        core::TextPosition{shown_line.number,
                           core::Column{core::source_column(shown_line.display, column.value - 1) +
                                        1 + shown_line.window.code_point}},
        hit.value().affinity};
}

// 見積もった仮想桁が窓の両端の外なら、索引の逆引きで本文の位置にする（ADR 0139 の決定 4）。
// End の x は float の最大、Home の x は最小なので、それぞれ行末と行頭に着く。
std::optional<core::TextCaret> VisualTextView::outside_window(const LineView &line,
                                                              float local) const
{
    if (!line.window.active)
    {
        return std::nullopt;
    }
    const auto estimate = static_cast<double>(line.window.column) +
                          (static_cast<double>(local) / static_cast<double>(advance_));
    if (local >= 0.0F && estimate < static_cast<double>(line.window.end_column))
    {
        return std::nullopt;
    }
    const auto &text = state_.text();
    const auto *index = giant_line_index(text, line.number);
    if (index == nullptr)
    {
        return std::nullopt;
    }
    const auto limit = static_cast<double>(text.size_bytes()) * 8.0;
    const auto column = static_cast<std::size_t>(std::clamp(estimate, 0.0, limit));
    const auto offset = index->byte_at_virtual_column(text, line.number, column);
    return core::TextCaret{giant_position_of(*index, text, line.number, offset)};
}

std::expected<core::VimRowSpan, TextLayoutFailure>
VisualTextView::row_span(const core::VimRowRequest &request)
{
    const auto &text = state_.text();
    const core::LineNumber number{
        std::clamp<std::size_t>(request.line.value, 1, text.line_count())};
    const auto count = rows(number);
    if (!count)
    {
        return std::unexpected(count.error());
    }
    const auto *const at = std::get_if<core::Offset>(&request.target);
    const auto row = at != nullptr ? row_of(number, *at)
                                   : std::expected<std::size_t, TextLayoutFailure>{std::min(
                                         std::get<std::size_t>(request.target), count.value() - 1)};
    if (!row)
    {
        return std::unexpected(row.error());
    }
    const auto start = row_start(number, row.value());
    const auto end = row.value() + 1 < count.value()
                         ? row_start(number, row.value() + 1)
                         : std::expected<core::Offset, TextLayoutFailure>{text.line_end(number)};
    if (!start || !end)
    {
        return std::unexpected(!start ? start.error() : end.error());
    }
    // 画面行の始まりと終わりと問いの位置の仮想桁は索引から引く（O(塊)・ADR 0144 の決定 3 の追補・
    // ADR 0145 の決定 3）。行を写さない。
    const auto start_column = core::virtual_column(text, start.value()).value - 1;
    const auto end_column = core::virtual_column(text, end.value()).value - 1;
    const auto column = at != nullptr ? core::virtual_column(text, *at).value - 1 : start_column;
    return core::VimRowSpan{number,
                            row.value(),
                            start.value(),
                            end.value(),
                            count.value(),
                            start_column,
                            end_column - std::min(end_column, start_column),
                            column};
}

std::size_t VisualTextView::horizontal_columns() const
{
    if (!scrolls_horizontally(caret_.line))
    {
        return 0;
    }
    return std::max<std::size_t>(
        static_cast<std::size_t>(static_cast<float>(geometry_.width) / advance_), 1);
}

// 升 column の x。巨大な行は origin からの見積もり（決定 9）、普通の行はその升を含む文字の x に、
// 文字の中の升の分だけ平均の字幅を足す（左端で半分欠けた全角の leftcol も同じ値に戻る）。
std::expected<float, TextLayoutFailure>
VisualTextView::horizontal_of_column(core::LineNumber number, std::size_t column)
{
    const auto &text = state_.text();
    if (giant_line_index(text, number) != nullptr)
    {
        return window_x(column, origin_, advance_);
    }
    const auto offset =
        core::offset_at_virtual_column(text, number, core::VirtualColumn{column + 1});
    const auto start = core::virtual_column(text, offset).value - 1;
    const auto located = location(core::TextCaret{text.position_of(offset)});
    if (!located)
    {
        return std::unexpected(located.error());
    }
    return located.value().x + (static_cast<float>(column - std::min(column, start)) * advance_);
}

// horizontal_of_column の逆。x を叩いた文字が x より右から始まるなら前の文字へ戻し、その文字の升に
// 文字の中の升（平均の字幅で割って丸める）を足す。
std::expected<std::size_t, TextLayoutFailure>
VisualTextView::column_of_horizontal(core::LineNumber number, float horizontal)
{
    const auto &text = state_.text();
    if (giant_line_index(text, number) != nullptr)
    {
        return origin_ + static_cast<std::size_t>((std::max(horizontal, 0.0F) / advance_) + 0.5F);
    }
    const auto hit = caret_at(core::VisualPosition{number, 0}, horizontal);
    if (!hit)
    {
        return std::unexpected(hit.error());
    }
    const auto line_start = text.line_start(number);
    auto offset = text.offset_of(hit.value().position);
    auto located = location(core::TextCaret{text.position_of(offset)});
    // 戻すときだけ行を 1 回写す（巨大な行はここへ来ない・戻す周ごとには写さない）。
    std::string content;
    while (located && located.value().x > horizontal && line_start < offset)
    {
        if (content.empty())
        {
            content = text.line_text(number);
        }
        offset = core::Offset{
            line_start.value +
            core::vim_character_start(content, core::Offset{offset.value - line_start.value})
                .value};
        located = location(core::TextCaret{text.position_of(offset)});
    }
    if (!located)
    {
        return std::unexpected(located.error());
    }
    const auto inside = std::max(horizontal - located.value().x, 0.0F) / advance_;
    return core::virtual_column(text, offset).value - 1 + static_cast<std::size_t>(inside + 0.5F);
}

std::expected<float, TextLayoutFailure> VisualTextView::followed_horizontal(core::TextCaret caret)
{
    if (!scrolls_horizontally(caret.position.line))
    {
        return 0.0F;
    }
    const auto located = location(caret);
    if (!located)
    {
        return std::unexpected(located.error());
    }
    return horizontal_for(caret, located.value().x);
}

bool VisualTextView::scrolls_horizontally(core::LineNumber number) const
{
    const bool giant = giant_line_index(state_.text(), number) != nullptr;
    return !(state_.settings().wrapping == core::LineWrapping::wrap && !giant) &&
           geometry_.width > 0 && !gridded();
}

// 画面行の頭は x の最小で叩いた位置（格子は升 0）。行の最初の画面行は行頭。
std::expected<core::Offset, TextLayoutFailure> VisualTextView::row_start(core::LineNumber number,
                                                                         std::size_t row)
{
    const auto &text = state_.text();
    if (row == 0)
    {
        return text.line_start(number);
    }
    if (gridded())
    {
        auto cells = grid_cells();
        if (!cells)
        {
            return std::unexpected(cells.error());
        }
        return cells.value().byte_of(number, core::GridPoint{row, 0});
    }
    const auto caret =
        caret_at(core::VisualPosition{number, row}, std::numeric_limits<float>::lowest());
    if (!caret)
    {
        return std::unexpected(caret.error());
    }
    return text.offset_of(caret.value().position);
}

std::expected<std::size_t, TextLayoutFailure> VisualTextView::row_of(core::LineNumber number,
                                                                     core::Offset at)
{
    const auto &text = state_.text();
    if (gridded())
    {
        auto cells = grid_cells();
        if (!cells)
        {
            return std::unexpected(cells.error());
        }
        return cells.value().point_of(number, at).row;
    }
    const auto located = location(core::TextCaret{text.position_of(at)});
    if (!located)
    {
        return std::unexpected(located.error());
    }
    return located.value().row;
}

std::expected<core::VisualPosition, TextLayoutFailure> VisualTextView::last()
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
VisualTextView::forward(core::VisualPosition from, std::size_t amount)
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
VisualTextView::backward(core::VisualPosition from, std::size_t amount)
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
VisualTextView::shifted(core::VisualPosition from, std::int64_t amount)
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
VisualTextView::constrained(core::VisualPosition first, core::ScrollExtent extent)
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

std::expected<std::size_t, TextLayoutFailure>
VisualTextView::distance(core::VisualPosition first, core::VisualPosition last, std::size_t limit)
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
VisualTextView::followed_first(core::VisualPosition caret, core::ScrollFollow follow)
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

std::expected<ScrollState, TextLayoutFailure> VisualTextView::followed(core::TextCaret caret,
                                                                       core::ScrollFollow follow,
                                                                       core::ScrollExtent extent)
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
    result.horizontal = horizontal_for(caret, point.value().x);
    return result;
}

// 格子は折り返すので横の送りは無い（ADR 0140 の決定 7）。巨大な行は折り返す設定でも横に
// 追う（ADR 0139 の決定 5）。
float VisualTextView::horizontal_for(core::TextCaret caret, float located) const
{
    if (!scrolls_horizontally(caret.position.line))
    {
        return 0;
    }
    const bool giant = giant_line_index(state_.text(), caret.position.line) != nullptr;
    const auto width = static_cast<float>(std::max(geometry_.width - 3, 1));
    const auto x = std::max(located, 0.0F);
    return giant ? giant_horizontal(caret, x, width)
                 : std::clamp(state_.scroll().horizontal, std::max(x - width, 0.0F), x);
}

// 窓は錨の刻みで替わるので、前の送りはキャレットの窓の隣の窓からの相対かもしれない。前の送りと
// その読み替えのそれぞれを、キャレットが見える最も近い送りへ寄せ、動きの最も小さいものを採る
// （窓が替わっても画面は 1 字ぶんしか動かない・ADR 0139 の決定 5）。
float VisualTextView::giant_horizontal(core::TextCaret caret, float x, float width) const
{
    const auto before = state_.scroll().horizontal;
    const auto low = std::max(x - width, 0.0F);
    auto best = std::clamp(before, low, x);
    const auto &text = state_.text();
    const auto *index = giant_line_index(text, caret.position.line);
    if (index == nullptr || caret.position.line != caret_.line || best == before)
    {
        return best;
    }
    const auto distance = [](float from, float to) { return from < to ? to - from : from - to; };
    auto movement = distance(before, best);
    const auto center = index->code_point_at(text, state_.selection().caret) /
                        core::giant_line_step * core::giant_line_step;
    const std::array<std::size_t, 2> neighbors{
        center + core::giant_line_step,
        center >= core::giant_line_step ? center - core::giant_line_step : center};
    for (const auto ordinal : neighbors)
    {
        const auto anchor = index->byte_at_code_point(text, caret.position.line, ordinal);
        const auto previous = giant_window_at(*index, text, caret.position.line, anchor).first;
        const auto shifted = before + window_x(previous.column, origin_, advance_);
        const auto target = std::clamp(shifted, low, x);
        if (distance(shifted, target) < movement)
        {
            best = target;
            movement = distance(shifted, target);
        }
    }
    return best;
}
} // namespace nenenib::application
