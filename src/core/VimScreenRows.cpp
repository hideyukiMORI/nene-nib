#include "VimScreenRows.hpp"

#include "Utf8.hpp"
#include "VimCaret.hpp"
#include "VimColumnWish.hpp"
#include "VimHorizontalScroll.hpp"
#include "VimMotionLanding.hpp"
#include "VimRepeatFailure.hpp"
#include "VimStep.hpp"
#include "VimViewport.hpp"
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

// 画面行 span の中で、行頭からの仮想桁 target（0 始まり）を占める文字の先頭（決定 3）。
// 届かなければ画面行の最後の文字で、行の最後の画面行なら行の内容の終わり（モードの寄せは
// 呼び手）。target が無ければ画面行の末尾（$ の後）。画面行が 1 つの行は j k と同じ論理行の道で
// 解き（索引を持つ本文は索引から・巨大な行の窓の形で行を写さない）、画面行の範囲を読むのは
// 画面行が 2 つ以上の行だけ。
[[nodiscard]] Offset landed(const TextBuffer &text, const VimRowSpan &span,
                            std::optional<std::size_t> target)
{
    if (span.rows <= 1)
    {
        return target.has_value()
                   ? offset_at_virtual_column(text, span.line, VirtualColumn{target.value() + 1})
                   : span.end;
    }
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

// 欲しい列（curswant・行頭からの仮想桁・0 始まり）。無ければ今のキャレットの桁、$ の後は無し。
[[nodiscard]] std::optional<std::size_t> wanted_of(const std::optional<VimWantedColumn> &column,
                                                   const VimEditorView &view)
{
    if (column.has_value() && column.value().wish == VimColumnWish::at_line_end)
    {
        return std::nullopt;
    }
    const auto wanted = column.has_value() ? column.value().column
                                           : caret_virtual_column(view.text, view.selection.caret);
    return std::max<std::size_t>(wanted.value, 1) - 1;
}

// gj gk の最初の欲しい列（ADR 0144 の決定 10・Vim 9.1 の nv_screengo）。今の行を画面行で埋めた
// 長さ n = 画面行数 × 1 画面行の升の数を越える欲しい列を n − 1 に切る。升の数が 0 なら切らない。
// $ の後（無し）は切らない。
[[nodiscard]] std::optional<std::size_t> screen_wanted(const std::optional<VimWantedColumn> &column,
                                                       const VimEditorView &view,
                                                       const VimRowSpan &from)
{
    const auto wanted = wanted_of(column, view);
    const auto filled = from.rows * view.layout.row_columns;
    if (!wanted.has_value() || filled == 0)
    {
        return wanted;
    }
    return std::min(wanted.value(), filled - 1);
}

// 行き先の画面行 to の中の欲しい列（決定 3 の追補・Vim の nv_screengo の curswant）。欲しい列
// column から今のキャレットの画面行 from の始まりの桁を引き、0 未満は 0、to の始まりの桁に足す。
// 画面行の幅を越える分は landed が画面行の最後の文字へ寄せる。$ の後（無し）は無し。
[[nodiscard]] std::optional<std::size_t> wanted_in(std::optional<std::size_t> column,
                                                   const VimRowSpan &from, const VimRowSpan &to)
{
    if (!column.has_value())
    {
        return std::nullopt;
    }
    const auto value = column.value();
    return to.start_column + (value > from.start_column ? value - from.start_column : 0);
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
    return vim_row_span_of(view, VimRowRequest{target.line, target.row});
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
    const auto caret_row = vim_row_span_of(view, VimRowRequest{caret_line, view.selection.caret});
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
        landed(view.text, row.value(),
               wanted_in(wanted_of(state.wanted_column, view), origin.second, row.value()))};
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

// 画面行 span の終わりの仮想桁（0 始まり・幅 0 の文字は数えない）。読むのは画面行の範囲だけで、
// 呼ぶのは画面行が 2 つ以上の行の途中の画面行だけ（kept_column）。
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

// gj gk で着いた後に残す欲しい列（行頭からの仮想桁・0 始まり）。行き先の桁 target を保つ。
// 行の途中の画面行（幅いっぱいまで埋まる）より右なら、その画面行の最後の桁（幅 − 1）に収める
// （Vim は curswant を画面行の幅で切ってから歩く・`k` の後の `gj`）。行の最後の画面行は短くても
// target を保つ（Vim 9.1 の実測・短い最後の画面行に着いた後の `j` は元の桁へ行く）。画面行が
// 1 つの行は最後の画面行なので、画面行の範囲を読まない。
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

// g0 gm g$ g^ の着地（ADR 0147 の決定 2・4・8）。g^ は g0 の所 at から最初の非空白へ進む。欲しい
// 列は着地の桁で、失敗は無い。
[[nodiscard]] VimRowLanding edge_landing(const TextBuffer &text, Offset at, VimDisplayLineEdge edge)
{
    const Offset landed =
        edge == VimDisplayLineEdge::first_non_blank ? vim_non_blank_from(text, at) : at;
    return VimRowLanding{
        VimMotionLanding{landed},
        VimWantedColumn{VimColumnWish::at_column, caret_virtual_column(text, landed)}};
}

// 横に送る文書（columns > 0）の g0 gm g$。見えている升 [left, left + columns) の左端の升・中の升を
// 含む文字と、右端の升に収まる最後の文字。行が見えている升より短ければ行の内容の終わり（モードの
// 寄せは呼び手・NORMAL は最後の文字）。左端で割れた全角はその字に着き、横位置は application の
// 最小追従が戻す（決定 6）。
[[nodiscard]] Offset scrolled_edge(const TextBuffer &text, const VimViewport &viewport,
                                   LineNumber line, VimDisplayLineEdge edge)
{
    const std::size_t left = viewport.left_column;
    switch (edge)
    {
    case VimDisplayLineEdge::start:
    case VimDisplayLineEdge::first_non_blank:
        return character_at_cell(text, line, left);
    case VimDisplayLineEdge::middle:
        return character_at_cell(text, line, left + (viewport.columns / 2));
    case VimDisplayLineEdge::end:
        break;
    }
    const std::size_t right = left + viewport.columns - 1;
    const std::pair<Offset, Offset> bounds{text.line_start(line), text.line_end(line)};
    const Offset at = character_at_cell(text, line, right);
    return at == bounds.second ? at : right_edge_character(text, bounds, right);
}

// 折り返す文書（layout.enabled）の g0 gm と回数 1 の g$。今の画面行の始まり・始まり + 1 画面行の
// 升の半分（短い段では最後の文字）・終わり（最後の段は行の内容の終わり）。
[[nodiscard]] Offset wrapped_edge(const VimEditorView &view, const VimRowSpan &span,
                                  VimDisplayLineEdge edge)
{
    switch (edge)
    {
    case VimDisplayLineEdge::start:
    case VimDisplayLineEdge::first_non_blank:
        return landed(view.text, span, span.start_column);
    case VimDisplayLineEdge::middle:
        return landed(view.text, span, span.start_column + (view.layout.row_columns / 2));
    case VimDisplayLineEdge::end:
        break;
    }
    return landed(view.text, span, std::nullopt);
}

// 幅も文字組みも無い文書の g0 gm g$（決定 2）。g0 gm は 0、g$ は回数 − 1 行下の $。
[[nodiscard]] Offset unlaid_edge(const TextBuffer &text, LineNumber line, VimDisplayLineEdge edge)
{
    switch (edge)
    {
    case VimDisplayLineEdge::start:
    case VimDisplayLineEdge::middle:
    case VimDisplayLineEdge::first_non_blank:
        return text.line_start(line);
    case VimDisplayLineEdge::end:
        break;
    }
    return text.line_end(line);
}
} // namespace

std::expected<VimRowSpan, VimLayoutNeed> vim_row_span_of(const VimEditorView &view,
                                                         const VimRowRequest &request)
{
    const auto found = std::ranges::find_if(view.layout.spans, [&request](const VimRowSpan &span)
                                            { return answers(span, request); });
    if (found == view.layout.spans.end())
    {
        return std::unexpected(VimLayoutNeed{request});
    }
    return *found;
}

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

std::expected<VimRowLanding, VimLayoutNeed>
vim_row_landing(const VimEditorView &view, VimScrollDirection direction, std::size_t amount,
                const std::optional<VimWantedColumn> &wanted)
{
    const Offset caret = view.selection.caret;
    const auto caret_row =
        vim_row_span_of(view, VimRowRequest{view.text.position_of(caret).line, caret});
    if (!caret_row)
    {
        return std::unexpected(caret_row.error());
    }
    const auto from = caret_row.value();
    const auto moved = walked(view, VisualPosition{from.line, from.row}, amount, direction);
    const auto row = moved ? vim_row_span_of(view, VimRowRequest{moved.value().first.line,
                                                                 moved.value().first.row})
                           : Needed<VimRowSpan>{std::unexpected(moved.error())};
    if (!row)
    {
        return std::unexpected(row.error());
    }
    const auto failure =
        moved.value().second < amount ? std::optional{VimRepeatFailure::not_moved} : std::nullopt;
    const auto target = wanted_in(screen_wanted(wanted, view, from), from, row.value());
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

std::expected<VimRowLanding, VimLayoutNeed>
vim_display_line_landing(const VimEditorView &view, VimDisplayLineEdge edge, std::size_t count)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    const LineNumber line = text.position_of(caret).line;
    const VimViewport viewport = vim_viewport_of(view);
    // {N}g$ は回数 − 1 だけ下へ。横に送る文書と文字組みの無い文書は N − 1 行下（本文の末尾で止まり
    // 失敗しない）、折り返す文書は N − 1 画面行下へ MAXCOL で歩く（決定 3・4）。
    const std::size_t below =
        edge == VimDisplayLineEdge::end ? std::max<std::size_t>(count, 1) - 1 : 0;
    const LineNumber target{line.value + std::min(below, text.line_count() - line.value)};
    if (viewport.columns > 0)
    {
        return edge_landing(text, scrolled_edge(text, viewport, target, edge), edge);
    }
    if (!view.layout.enabled)
    {
        return edge_landing(text, unlaid_edge(text, target, edge), edge);
    }
    if (below > 0)
    {
        return vim_row_landing(view, VimScrollDirection::down, below,
                               VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}});
    }
    const auto span = vim_row_span_of(view, VimRowRequest{line, caret});
    if (!span)
    {
        return std::unexpected(span.error());
    }
    return edge_landing(text, wrapped_edge(view, span.value(), edge), edge);
}

std::expected<VisualPosition, VimLayoutNeed> vim_row_aligned_top(const VimEditorView &view,
                                                                 Offset caret, std::size_t above)
{
    const auto span =
        vim_row_span_of(view, VimRowRequest{view.text.position_of(caret).line, caret});
    if (!span)
    {
        return std::unexpected(span.error());
    }
    const auto walk = backward(view, VisualPosition{span.value().line, span.value().row}, above);
    if (!walk)
    {
        return std::unexpected(walk.error());
    }
    return walk.value().first;
}

std::expected<LineNumber, VimLayoutNeed> vim_row_below_window(const VimEditorView &view)
{
    const auto viewport = vim_viewport_of(view);
    const LineNumber first{
        std::clamp<std::size_t>(viewport.first_visible.value, 1, view.text.line_count())};
    const auto height = std::max<std::size_t>(viewport.visible_lines, 1);
    const auto below = forward(view, VisualPosition{first, viewport.first_row}, height);
    if (!below)
    {
        return std::unexpected(below.error());
    }
    return below.value().first.line;
}
} // namespace nenenib::core
