#include "VimHorizontalScroll.hpp"

#include "LineNumber.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VimActionGroup.hpp"
#include "VimCaret.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimKeyTable.hpp"
#include "VimRowRequest.hpp"
#include "VimRowSpan.hpp"
#include "VimScreenRows.hpp"
#include "VimStep.hpp"
#include "VimViewport.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <tuple>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr char32_t tab_character = U'\t';
// 1 つの code point の UTF-8 の最長のバイト数。1 文字の頭の code point だけを読む。
constexpr std::size_t longest_code_point_bytes = 4;
// 次の文字の先頭を探すときに読む最初の窓（結合文字の列が長ければ倍にして読み直す）。
constexpr std::size_t character_window_bytes = 64;

// 行の内容の範囲 [start, end)。
using LineBounds = std::pair<Offset, Offset>;

[[nodiscard]] bool scrolls_horizontally(VimAction action) noexcept
{
    return vim_group_for(action) == VimActionGroup::horizontal_scroll;
}

[[nodiscard]] bool scrolls_right(VimAction action) noexcept
{
    return action == VimAction::scroll_column_right || action == VimAction::scroll_half_width_right;
}

[[nodiscard]] bool pushes_caret(VimAction action) noexcept
{
    return action != VimAction::scroll_caret_to_left && action != VimAction::scroll_caret_to_right;
}

// at から始まる文字の頭の code point（行の内容の終わりでは 0）。
[[nodiscard]] char32_t code_point_of(const TextBuffer &text, Offset at, Offset line_end)
{
    if (at.value >= line_end.value)
    {
        return 0;
    }
    const std::string bytes =
        text.text_range(at, Offset{std::min(at.value + longest_code_point_bytes, line_end.value)});
    return code_point_at(bytes, Offset{0});
}

// 1 文字が占める升の数。幅 0 の文字（行頭の孤立した結合文字）と行の内容の終わりも 1 升に数える。
[[nodiscard]] std::size_t cells_of(char32_t value, std::size_t column) noexcept
{
    return std::max<std::size_t>(column_advance(value, column), 1);
}

// at から始まる Vim の 1 文字の次の文字の先頭（ADR 0053）。at の後ろの窓だけを読み、文字の終わりが
// 窓の終わりに触れていれば窓を倍にして読み直す。
[[nodiscard]] Offset character_after(const TextBuffer &text, Offset at, Offset line_end)
{
    std::size_t window = character_window_bytes;
    for (;;)
    {
        const Offset end{std::min(at.value + window, line_end.value)};
        const std::string content = text.text_range(at, end);
        const Offset next = vim_character_end(content, Offset{0});
        if (end == line_end || next.value + longest_code_point_bytes <= content.size())
        {
            return Offset{at.value + next.value};
        }
        window *= 2;
    }
}

// 新しい leftcol の上限。キャレットの行の最後の文字の最初の升（Vim 9.1 の実測・最後の文字が
// 左端に残る所まで）。幅は画面行の答えの width で、最後の文字の幅は 1 文字だけ読んで引く。Tab の
// 幅は始まりの桁に依るので、Tab だけはその桁を索引（無ければ行）から引く。
[[nodiscard]] std::size_t upper_column(const TextBuffer &text, LineBounds line,
                                       const VimRowSpan &span)
{
    if (line.second.value <= line.first.value)
    {
        return 0;
    }
    const Offset last = vim_resting_caret(text, line.second);
    const char32_t value = code_point_of(text, last, line.second);
    if (value == tab_character)
    {
        return virtual_column(text, last).value - 1;
    }
    const std::size_t width = cells_of(value, 0);
    return span.width >= width ? span.width - width : 0;
}

[[nodiscard]] std::size_t scrolled_amount(VimAction action, std::size_t count, std::size_t columns)
{
    const bool half =
        action == VimAction::scroll_half_width_right || action == VimAction::scroll_half_width_left;
    const std::size_t unit = half ? columns / 2 : 1;
    if (unit == 0 || count == 0)
    {
        return 0;
    }
    return count > std::numeric_limits<std::size_t>::max() / unit
               ? std::numeric_limits<std::size_t>::max()
               : count * unit;
}

// キャレットの文字が占める最初と最後の升と、Vim がキャレットを描く升（Tab だけは最後の升・
// ADR 0034 の決定 3）。添字は 0 が最初の升（first）、1 が最後の升（last）、2 が描く升（shown）。
// 型を足さず tuple で持つ（1 ファイル 1 型・CPP-011）。
using CaretCells = std::tuple<std::size_t, std::size_t, std::size_t>;

// 押す前の新しい leftcol（Vim の nv_zet の zl zh zL zH zs ze）。zs はキャレットの文字の最初の升、
// ze は最後の升が右端に来る所（0 未満は 0）。
[[nodiscard]] std::size_t requested_column(VimAction action, std::size_t count,
                                           const VimViewport &viewport, const CaretCells &caret)
{
    if (action == VimAction::scroll_caret_to_left)
    {
        return std::get<0>(caret);
    }
    const std::size_t columns = viewport.columns;
    if (action == VimAction::scroll_caret_to_right)
    {
        return std::get<1>(caret) + 1 > columns ? std::get<1>(caret) + 1 - columns : 0;
    }
    const std::size_t amount = scrolled_amount(action, count, columns);
    const std::size_t left = viewport.left_column;
    if (scrolls_right(action))
    {
        return left > std::numeric_limits<std::size_t>::max() - amount
                   ? std::numeric_limits<std::size_t>::max()
                   : left + amount;
    }
    return left - std::min(left, amount);
}

// 左端の升 left を含む文字。そこで始まらなければ次の文字。
[[nodiscard]] Offset left_edge_character(const TextBuffer &text, LineBounds line, std::size_t left)
{
    const LineNumber number = text.position_of(line.first).line;
    const Offset at = character_at_cell(text, number, left);
    const bool starts = left == 0 || character_at_cell(text, number, left - 1) != at;
    return starts ? vim_resting_caret(text, at)
                  : vim_resting_caret(text, character_after(text, at, line.second));
}

// 画面 [left, left + columns) の外に出たキャレットを端の文字へ押す（Vim の leftcol_changed・
// sidescrolloff=0）。押さなければ無し。
[[nodiscard]] std::optional<Offset> pushed_caret(const TextBuffer &text, LineBounds line,
                                                 std::pair<std::size_t, std::size_t> window,
                                                 const CaretCells &caret)
{
    const auto [first, last, shown] = caret;
    const std::size_t right = window.first + window.second - 1;
    if (shown > right || last > right)
    {
        return right_edge_character(text, line, right);
    }
    if (shown < window.first || first < window.first)
    {
        return left_edge_character(text, line, window.first);
    }
    return std::nullopt;
}
} // namespace

// 公開の 2 本（ADR 0147 の決定 2・g0 gm g$ と共用）。宣言は VimHorizontalScroll.hpp。
Offset character_at_cell(const TextBuffer &text, LineNumber line, std::size_t column)
{
    return offset_at_virtual_column(text, line, VirtualColumn{column + 1});
}

Offset right_edge_character(const TextBuffer &text, std::pair<Offset, Offset> line,
                            std::size_t right)
{
    const LineNumber number = text.position_of(line.first).line;
    const Offset at = character_at_cell(text, number, right);
    const bool ends = character_at_cell(text, number, right + 1) != at;
    return ends || at == line.first ? vim_resting_caret(text, at)
                                    : vim_resting_caret(text, Offset{at.value - 1});
}

std::expected<std::optional<VimHorizontalScroll>, VimLayoutNeed>
vim_horizontal_scroll(const VimState &state, const VimEditorView &view, VimAction action)
{
    const VimViewport viewport = vim_viewport_of(view);
    if (!scrolls_horizontally(action) || viewport.columns == 0)
    {
        return std::nullopt;
    }
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    const LineNumber number = text.position_of(caret).line;
    const auto span = vim_row_span_of(view, VimRowRequest{number, caret});
    if (!span)
    {
        return std::unexpected(span.error());
    }
    const LineBounds line{text.line_start(number), text.line_end(number)};
    const char32_t value = code_point_of(text, caret, line.second);
    const std::size_t first = span.value().column;
    const std::size_t last = first + cells_of(value, first) - 1;
    const CaretCells cells{first, last, value == tab_character ? last : first};
    const std::size_t count = state.count.has_value() ? state.count.value().value : 1;
    const std::size_t left = std::min(requested_column(action, count, viewport, cells),
                                      upper_column(text, line, span.value()));
    if (left == viewport.left_column || !pushes_caret(action))
    {
        return std::optional{VimHorizontalScroll{left, caret, false}};
    }
    const auto pushed = pushed_caret(text, line, {left, viewport.columns}, cells);
    return std::optional{VimHorizontalScroll{left, pushed.value_or(caret), pushed.has_value()}};
}
} // namespace nenenib::core
