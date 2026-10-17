#include "VimScreenRows.hpp"

#include "Utf8.hpp"
#include "VimColumnWish.hpp"
#include "VimMotionLanding.hpp"
#include "VimRepeatFailure.hpp"
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

// 画面行 span の中で、行頭からの仮想桁 target（0 始まり）を占める文字の先頭（決定 3）。読むのは
// 画面行の範囲だけで、桁は span.start_column から足す。届かなければ画面行の最後の文字で、行の最後の
// 画面行なら行の内容の終わり（モードの寄せは呼び手）。target が無ければ画面行の末尾（$ の後）。
[[nodiscard]] Offset landed(const TextBuffer &text, const VimRowSpan &span,
                            std::optional<std::size_t> target)
{
    const std::string shown = text.text_range(span.start, span.end);
    std::size_t column = span.start_column;
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

// 行き先の画面行 to の中の欲しい列（決定 3 の追補・Vim の nv_screengo の curswant）。欲しい列
// （無ければ今のキャレットの桁）から今のキャレットの画面行 from の始まりの桁を引き、0 未満は 0、
// to の始まりの桁に足す。画面行の幅を越える分は landed が画面行の最後の文字へ寄せる。$ の後は無し。
[[nodiscard]] std::optional<std::size_t> wanted_in(const VimState &state, const VimEditorView &view,
                                                   const VimRowSpan &from, const VimRowSpan &to)
{
    if (state.wanted_column.has_value() &&
        state.wanted_column.value().wish == VimColumnWish::at_line_end)
    {
        return std::nullopt;
    }
    const auto wanted = state.wanted_column.has_value()
                            ? state.wanted_column.value().column
                            : caret_virtual_column(view.text, view.selection.caret);
    const std::size_t column = std::max<std::size_t>(wanted.value, 1) - 1;
    return to.start_column + (column > from.start_column ? column - from.start_column : 0);
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

// 送る前の画面の先頭（画面行に丸めた位置）と、キャレットの画面行。
using Origin = std::pair<VisualPosition, VimRowSpan>;
// 先頭の歩きとキャレットの歩き。
using Moves = std::pair<Walk, Walk>;

[[nodiscard]] Needed<Origin> origin_of(const VimEditorView &view)
{
    const auto viewport = vim_viewport_of(view);
    const LineNumber first{
        std::clamp<std::size_t>(viewport.first_visible.value, 1, view.text.line_count())};
    const LineNumber caret_line = view.text.position_of(view.selection.caret).line;
    const auto top = forward(view, VisualPosition{first, viewport.first_row}, 0);
    const auto caret_row = span_of(view, VimRowRequest{caret_line, view.selection.caret});
    if (!top || !caret_row)
    {
        return std::unexpected(!top ? top.error() : caret_row.error());
    }
    return Origin{top.value().first, caret_row.value()};
}

[[nodiscard]] Needed<Walk> caret_walked(const VimEditorView &view, const Origin &origin,
                                        std::size_t amount, VimScrollDirection direction)
{
    const auto &caret = origin.second;
    return walked(view, VisualPosition{caret.line, caret.row}, amount, direction);
}

// 先頭とキャレットの歩きから、画面の先頭とキャレットの置き所を決める。どちらも動かなければ何も
// 変えない（失敗にしない）。
[[nodiscard]] Needed<VimDisplayNavigation> settled(const VimState &state, const VimEditorView &view,
                                                   const Origin &origin, const Moves &moves)
{
    const auto &[top, caret] = moves;
    if (top.second == 0 && caret.second == 0)
    {
        return VimDisplayNavigation{origin.first.line, origin.second.line, true,
                                    std::nullopt,      origin.first.row,   view.selection.caret};
    }
    const auto row = target_row(view, caret.first, top.first);
    if (!row)
    {
        return std::unexpected(row.error());
    }
    return VimDisplayNavigation{
        top.first.line,
        row.value().line,
        false,
        std::nullopt,
        top.first.row,
        landed(view.text, row.value(), wanted_in(state, view, origin.second, row.value()))};
}

// 半画面の先頭の歩き（決定 5 の追補・Vim の CTRL-D）。下へは最後の画面行が画面に見えたら止め、
// 上へは最初の画面行で止める。
[[nodiscard]] Needed<Walk> half_top(const VimEditorView &view, VisualPosition top,
                                    std::size_t amount, VimScrollDirection direction)
{
    switch (direction)
    {
    case VimScrollDirection::up:
        return backward(view, top, amount);
    case VimScrollDirection::down:
        break;
    }
    const auto height = std::max<std::size_t>(vim_viewport_of(view).visible_lines, 1);
    const auto bottom = forward(view, top, height - 1);
    const auto room = bottom ? forward(view, bottom.value().first, amount) : bottom;
    if (!room)
    {
        return std::unexpected(room.error());
    }
    return forward(view, top, room.value().second);
}

// 画面行 span の終わりの仮想桁（0 始まり・幅 0 の文字は数えない）。読むのは画面行の範囲だけ。
[[nodiscard]] std::size_t end_column(const TextBuffer &text, const VimRowSpan &span)
{
    const std::string shown = text.text_range(span.start, span.end);
    std::size_t column = span.start_column;
    for (Offset at{0}; at.value < shown.size(); at = next_code_point(shown, at))
    {
        column += column_advance(code_point_at(shown, at), column);
    }
    return column;
}

// gj gk で着いた後に残す欲しい列（行頭からの仮想桁・0 始まり）。行き先の桁 target
// を保つ。行の途中の 画面行（幅いっぱいまで埋まる）より右なら、その画面行の最後の桁に収める（Vim は
// curswant を画面行の 幅で切ってから歩く・`k` の後の `gj`）。行の最後の画面行は短くても target
// を保つ（Vim 9.1 の実測・ 短い最後の画面行に着いた後の `j` は元の桁へ行く）。
[[nodiscard]] std::size_t kept_column(const TextBuffer &text, const VimRowSpan &row,
                                      std::size_t target)
{
    if (row.row + 1 >= row.rows)
    {
        return target;
    }
    const auto end = end_column(text, row);
    return end > row.start_column && target >= end ? end - 1 : target;
}
} // namespace

std::expected<VimDisplayNavigation, VimLayoutNeed> vim_row_navigation(const VimState &state,
                                                                      const VimEditorView &view,
                                                                      VimScrollDirection direction,
                                                                      std::size_t amount)
{
    const auto origin = origin_of(view);
    const auto top =
        origin ? walked(view, origin.value().first, amount, direction) : Needed<Walk>{};
    if (!origin || !top)
    {
        return std::unexpected(!origin ? origin.error() : top.error());
    }
    const auto caret = caret_walked(view, origin.value(), top.value().second, direction);
    if (!caret)
    {
        return std::unexpected(caret.error());
    }
    return settled(state, view, origin.value(), Moves{top.value(), caret.value()});
}

std::expected<VimDisplayNavigation, VimLayoutNeed>
vim_row_half_navigation(const VimState &state, const VimEditorView &view,
                        VimScrollDirection direction, std::size_t amount)
{
    const auto origin = origin_of(view);
    const auto top =
        origin ? half_top(view, origin.value().first, amount, direction) : Needed<Walk>{};
    if (!origin || !top)
    {
        return std::unexpected(!origin ? origin.error() : top.error());
    }
    const auto caret = caret_walked(view, origin.value(), amount, direction);
    if (!caret)
    {
        return std::unexpected(caret.error());
    }
    return settled(state, view, origin.value(), Moves{top.value(), caret.value()});
}

std::expected<VimRowLanding, VimLayoutNeed> vim_row_landing(const VimState &state,
                                                            const VimEditorView &view,
                                                            VimScrollDirection direction,
                                                            std::size_t amount)
{
    const Offset caret = view.selection.caret;
    const auto caret_row = span_of(view, VimRowRequest{view.text.position_of(caret).line, caret});
    if (!caret_row)
    {
        return std::unexpected(caret_row.error());
    }
    const auto from = caret_row.value();
    const auto moved = walked(view, VisualPosition{from.line, from.row}, amount, direction);
    const auto row =
        moved ? span_of(view, VimRowRequest{moved.value().first.line, moved.value().first.row})
              : Needed<VimRowSpan>{std::unexpected(moved.error())};
    if (!row)
    {
        return std::unexpected(row.error());
    }
    const auto failure =
        moved.value().second < amount ? std::optional{VimRepeatFailure::not_moved} : std::nullopt;
    const auto target = wanted_in(state, view, from, row.value());
    const auto landing = VimMotionLanding{landed(view.text, row.value(), target), failure};
    if (!target.has_value())
    {
        return VimRowLanding{landing,
                             VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}}};
    }
    const auto kept = kept_column(view.text, row.value(), target.value());
    return VimRowLanding{landing,
                         VimWantedColumn{VimColumnWish::at_column, VirtualColumn{kept + 1}}};
}
} // namespace nenenib::core
