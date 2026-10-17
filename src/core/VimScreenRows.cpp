#include "VimScreenRows.hpp"

#include "Utf8.hpp"
#include "VimStep.hpp"
#include "VirtualColumn.hpp"
#include "VisualPosition.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
template <typename Value> using Needed = std::expected<Value, VimLayoutNeed>;
// 歩いた先の画面行と、実際に歩いた画面行の数。
using Walk = std::pair<VisualPosition, std::size_t>;

[[nodiscard]] Needed<std::size_t> rows_of(const VimEditorView &view, LineNumber line)
{
    const auto found =
        std::ranges::lower_bound(view.layout.lines, line.value, {},
                                 [](const VimLineRows &value) { return value.line.value; });
    if (found == view.layout.lines.end() || found->line != line)
    {
        return std::unexpected(VimLayoutNeed{line});
    }
    return std::max<std::size_t>(found->rows, 1);
}

// 答えの span が問いに当たるか。位置の問いは [start, end) に入るか、最後の画面行の行末。
[[nodiscard]] bool answers(const VimRowSpan &span, const VimRowRequest &request) noexcept
{
    if (span.line != request.line)
    {
        return false;
    }
    if (const auto *const at = std::get_if<Offset>(&request.target); at != nullptr)
    {
        return span.start.value <= at->value &&
               (at->value < span.end.value || span.row + 1 >= span.rows);
    }
    return span.row == std::get<std::size_t>(request.target);
}

[[nodiscard]] Needed<VimRowSpan> span_of(const VimEditorView &view, const VimRowRequest &request)
{
    const auto found = std::ranges::find_if(view.layout.spans, [&request](const VimRowSpan &span)
                                            { return answers(span, request); });
    if (found == view.layout.spans.end())
    {
        return std::unexpected(VimLayoutNeed{request});
    }
    return *found;
}

[[nodiscard]] Needed<Walk> forward(const VimEditorView &view, VisualPosition from,
                                   std::size_t amount)
{
    std::size_t moved = 0;
    while (true)
    {
        const auto rows = rows_of(view, from.line);
        if (!rows)
        {
            return std::unexpected(rows.error());
        }
        from.row = std::min(from.row, rows.value() - 1);
        const auto available = rows.value() - 1 - from.row;
        if (amount <= available)
        {
            return Walk{VisualPosition{from.line, from.row + amount}, moved + amount};
        }
        if (from.line.value >= view.text.line_count())
        {
            return Walk{VisualPosition{from.line, rows.value() - 1}, moved + available};
        }
        moved += available + 1;
        amount -= available + 1;
        ++from.line.value;
        from.row = 0;
    }
}

// 前の行へ移るときは行数をまだ知らないので、次の周で行数に丸める（その行の最後の画面行）。
[[nodiscard]] Needed<Walk> backward(const VimEditorView &view, VisualPosition from,
                                    std::size_t amount)
{
    std::size_t moved = 0;
    while (true)
    {
        const auto rows = rows_of(view, from.line);
        if (!rows)
        {
            return std::unexpected(rows.error());
        }
        from.row = std::min(from.row, rows.value() - 1);
        if (amount <= from.row)
        {
            return Walk{VisualPosition{from.line, from.row - amount}, moved + amount};
        }
        if (from.line.value <= 1)
        {
            return Walk{VisualPosition{LineNumber{1}, 0}, moved + from.row};
        }
        moved += from.row + 1;
        amount -= from.row + 1;
        --from.line.value;
        from.row = std::numeric_limits<std::size_t>::max();
    }
}

[[nodiscard]] Needed<Walk> walked(const VimEditorView &view, VisualPosition from,
                                  std::size_t amount, VimScrollDirection direction)
{
    switch (direction)
    {
    case VimScrollDirection::down:
        return forward(view, from, amount);
    case VimScrollDirection::up:
        return backward(view, from, amount);
    }
    std::unreachable();
}

// 行頭からの仮想桁（0 始まり）。索引を持つ本文は行を写さない（VirtualColumn の 1 本）。
[[nodiscard]] std::size_t column_before(const TextBuffer &text, Offset at)
{
    return virtual_column(text, at).value - 1;
}

// 画面行 span の中で、行頭からの仮想桁 target（0 始まり）を占める文字の先頭（決定 3）。読むのは
// 画面行の範囲だけ。届かなければ画面行の最後の文字で、行の最後の画面行なら行の内容の終わり
// （モードの寄せは呼び手）。target が無ければ画面行の末尾（$ の後）。
[[nodiscard]] Offset landed(const TextBuffer &text, const VimRowSpan &span,
                            std::optional<std::size_t> target)
{
    const std::string shown = text.text_range(span.start, span.end);
    std::size_t column = column_before(text, span.start);
    std::optional<std::size_t> last = std::nullopt;
    Offset at{0};
    while (at.value < shown.size())
    {
        const auto advance = column_advance(code_point_at(shown, at), column);
        if (advance != 0 && target.has_value() && target.value() < column + advance)
        {
            return Offset{span.start.value + at.value};
        }
        last = advance != 0 ? std::optional{at.value} : last;
        column += advance;
        at = next_code_point(shown, at);
    }
    if (span.row + 1 >= span.rows)
    {
        return span.end;
    }
    return Offset{span.start.value + last.value_or(0)};
}

// 欲しい列の画面行の中の仮想桁（0 始まり）。今のキャレットの画面行の中の桁で、$ の後は無し。
[[nodiscard]] std::optional<std::size_t> wish_in_row(const VimState &state, const TextBuffer &text,
                                                     const VimRowSpan &caret_row, Offset caret)
{
    if (state.wanted_column.has_value() &&
        state.wanted_column.value().wish == VimColumnWish::at_line_end)
    {
        return std::nullopt;
    }
    return column_before(text, caret) - column_before(text, caret_row.start);
}

[[nodiscard]] VisualPosition clamped(VisualPosition value, VisualPosition low,
                                     VisualPosition high) noexcept
{
    if (value < low)
    {
        return low;
    }
    return high < value ? high : value;
}

// 送った後の画面の最上段と最下段の間にキャレットの画面行を収め、その画面行の範囲を引く。
[[nodiscard]] Needed<VimRowSpan> target_row(const VimEditorView &view, VisualPosition caret,
                                            VisualPosition top)
{
    const auto height = std::max<std::size_t>(vim_viewport_of(view).visible_lines, 1);
    const auto bottom = forward(view, top, height - 1);
    if (!bottom)
    {
        return std::unexpected(bottom.error());
    }
    const auto target = clamped(caret, top, bottom.value().first);
    return span_of(view, VimRowRequest{target.line, target.row});
}
} // namespace

std::expected<VimDisplayNavigation, VimLayoutNeed> vim_row_navigation(const VimState &state,
                                                                      const VimEditorView &view,
                                                                      VimScrollDirection direction,
                                                                      std::size_t amount)
{
    const auto viewport = vim_viewport_of(view);
    const LineNumber first{
        std::clamp<std::size_t>(viewport.first_visible.value, 1, view.text.line_count())};
    const Offset caret = view.selection.caret;
    const LineNumber caret_line = view.text.position_of(caret).line;
    const auto top = walked(view, VisualPosition{first, viewport.first_row}, 0, direction);
    const auto caret_row = span_of(view, VimRowRequest{caret_line, caret});
    if (!top || !caret_row)
    {
        return std::unexpected(!top ? top.error() : caret_row.error());
    }
    const auto moved_top = walked(view, top.value().first, amount, direction);
    if (!moved_top)
    {
        return std::unexpected(moved_top.error());
    }
    const auto distance = moved_top.value().second;
    if (distance == 0)
    {
        return VimDisplayNavigation{first, caret_line, true, std::nullopt, top.value().first.row,
                                    caret};
    }
    const auto new_top = moved_top.value().first;
    const auto moved_caret =
        walked(view, VisualPosition{caret_line, caret_row.value().row}, distance, direction);
    if (!moved_caret)
    {
        return std::unexpected(moved_caret.error());
    }
    const auto row = target_row(view, moved_caret.value().first, new_top);
    if (!row)
    {
        return std::unexpected(row.error());
    }
    const auto wish = wish_in_row(state, view.text, caret_row.value(), caret);
    const auto wanted =
        wish.has_value() ? std::optional{column_before(view.text, row.value().start) + wish.value()}
                         : std::nullopt;
    return VimDisplayNavigation{new_top.line, row.value().line,
                                false,        std::nullopt,
                                new_top.row,  landed(view.text, row.value(), wanted)};
}
} // namespace nenenib::core
