#include "VimToplineUpdate.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <tuple>
#include <utility>

namespace nenenib::core
{
namespace
{
using Rows = std::int64_t;
// Vim の MAXCOL（本文の外の行の高さ）。足しても桁あふれしない大きさ。
constexpr Rows beyond = Rows{1} << 30;
// Vim の scrolljump（Nib に設定は無く Vim の既定の 1）。
constexpr Rows scroll_jump = 1;
// 余白は切り詰めない（Vim と同じ・so=999 は常に中央）が、数えて桁あふれしない上限で切る。
constexpr std::size_t margin_limit = std::size_t{1} << 20;

// 窓の先頭・窓に全部は入らない最初の行（Vim の w_botline）・下の空き段（w_empty_rows）。
using Shown = std::tuple<std::size_t, std::size_t, Rows>;
// scroll_cursor_bot の歩み: 上へ足した行・下へ足した行・使った段・送る段・下の余白。
using Walk = std::tuple<std::size_t, std::size_t, Rows, Rows, Rows>;
// scroll_cursor_halfway の片側: いちばん外の行と足した段。
using Side = std::pair<std::size_t, Rows>;
// 上の側と下の側。
using Halves = std::pair<Side, Side>;

[[nodiscard]] Rows height_of(const VimToplineWindow &window) noexcept
{
    return static_cast<Rows>(window.height);
}

[[nodiscard]] Rows margin_of(const VimToplineWindow &window) noexcept
{
    return static_cast<Rows>(std::min(window.margin, margin_limit));
}

// Vim の plines。本文の外は beyond、載っていない行は 1。
[[nodiscard]] Rows plines(const VimToplineWindow &window, std::size_t count,
                          std::size_t line) noexcept
{
    if (line < 1 || line > count)
    {
        return beyond;
    }
    const auto found = std::ranges::lower_bound(window.rows, line, {}, [](const VimLineRows &value)
                                                { return value.line.value; });
    if (found == window.rows.end() || found->line.value != line)
    {
        return 1;
    }
    return static_cast<Rows>(std::max<std::size_t>(found->rows, 1));
}

// topline_back の高さ（Vim の PLINES_WIN_NOFILL は窓の高さで切る・本文の上は beyond）。
[[nodiscard]] Rows back_plines(const VimToplineWindow &window, std::size_t count,
                               std::size_t line) noexcept
{
    const Rows rows = plines(window, count, line);
    return line < 1 ? rows : std::min(rows, std::max<Rows>(height_of(window), 1));
}

// check_top_offset: キャレットの上に余白の画面行が窓の中に無いか。
[[nodiscard]] bool top_offset_short(const VimToplineWindow &window, std::size_t count,
                                    std::size_t top, std::size_t cursor) noexcept
{
    const Rows margin = margin_of(window);
    if (cursor >= top + static_cast<std::size_t>(margin))
    {
        return false;
    }
    Rows above = 0;
    for (std::size_t line = cursor; above < margin && line > top; --line)
    {
        above += back_plines(window, count, line - 1);
    }
    return above < margin;
}

// scroll_cursor_halfway の下へ 1 行（本文の下は「~」の 1 段で、使った段は増やさない）。
[[nodiscard]] bool added_below(const VimToplineWindow &window, std::size_t count, Side &below,
                               Rows &used) noexcept
{
    if (below.first >= count)
    {
        ++below.second;
        return true;
    }
    const Rows rows = plines(window, count, below.first + 1);
    used += rows;
    if (used > height_of(window))
    {
        return false;
    }
    ++below.first;
    below.second += rows;
    return true;
}

[[nodiscard]] bool added_above(const VimToplineWindow &window, std::size_t count, Side &above,
                               Rows &used) noexcept
{
    const Rows rows = back_plines(window, count, above.first - 1);
    used += rows;
    if (used > height_of(window))
    {
        return false;
    }
    --above.first;
    above.second += rows;
    return true;
}

// 1 周で下を先に足す（Vim の prefer_above が偽: 下が上以下なら下、その後に下が上より多ければ上）。
[[nodiscard]] bool balanced_below_first(const VimToplineWindow &window, std::size_t count,
                                        Halves &halves, Rows &used) noexcept
{
    auto &[above, below] = halves;
    if (below.second <= above.second && !added_below(window, count, below, used))
    {
        return false;
    }
    return below.second <= above.second || added_above(window, count, above, used);
}

// 1 周で上を先に足す（prefer_above が真: 下が上以上なら上、その後に下が上より少なければ下）。
[[nodiscard]] bool balanced_above_first(const VimToplineWindow &window, std::size_t count,
                                        Halves &halves, Rows &used) noexcept
{
    auto &[above, below] = halves;
    if (below.second >= above.second && !added_above(window, count, above, used))
    {
        return false;
    }
    return below.second >= above.second || added_below(window, count, below, used);
}

// scroll_cursor_halfway（atend は偽）: キャレットの行から上と下へ交互に足し、窓を越える手前の
// 上の行を先頭にする。
[[nodiscard]] std::size_t cursor_halfway(const VimToplineWindow &window, std::size_t count,
                                         std::size_t cursor, bool prefer_above) noexcept
{
    Halves halves{Side{cursor, 0}, Side{cursor, 0}};
    Rows used = plines(window, count, cursor);
    while (halves.first.first > 1)
    {
        const bool going = prefer_above ? balanced_above_first(window, count, halves, used)
                                        : balanced_below_first(window, count, halves, used);
        if (!going)
        {
            break;
        }
    }
    return halves.first.first;
}

// 窓の先頭より上の行の段（scroll_cursor_top の scrolled に数える分）。
[[nodiscard]] Rows above_window(std::size_t line, std::size_t top, Rows rows) noexcept
{
    return line < top ? rows : 0;
}

// scroll_cursor_top が上の行と組にして下に足す行（余白がまだ要り、本文の最後の行より前のとき）。
[[nodiscard]] Rows paired_below(const VimToplineWindow &window, std::size_t count, std::size_t line,
                                bool wanted) noexcept
{
    return wanted && line < count ? plines(window, count, line) : 0;
}

// 上の行を足すのを止めるか（送りが scrolljump を越えたか先頭に届き、上の余白が足りた）。
[[nodiscard]] bool top_settled(bool reached, Rows scrolled, Rows extra, Rows margin) noexcept
{
    return (reached || scrolled > scroll_jump) && extra >= margin;
}

// scroll_cursor_top（always は偽）: キャレットの上に余白の行を置いた先頭。窓に収まらなければ
// scroll_cursor_halfway。先頭は下げない（折り返す行で上がるだけ）。
[[nodiscard]] std::size_t cursor_top(const VimToplineWindow &window, std::size_t count,
                                     std::size_t top, std::size_t cursor) noexcept
{
    const Rows margin = margin_of(window);
    Rows used = plines(window, count, cursor);
    Rows scrolled = above_window(cursor, top, used);
    Rows extra = 0;
    std::size_t above = cursor - 1;
    std::size_t below = cursor + 1;
    std::size_t reached = cursor;
    while (above > 0)
    {
        const Rows rows = plines(window, count, above);
        scrolled += above_window(above, top, rows);
        if (top_settled(reached >= top, scrolled, extra, margin))
        {
            break;
        }
        used += rows + paired_below(window, count, below, extra + rows <= margin);
        if (used > height_of(window))
        {
            break;
        }
        extra += rows;
        reached = above;
        --above;
        ++below;
    }
    if (used > height_of(window))
    {
        return cursor_halfway(window, count, cursor, false);
    }
    return std::min(std::min(reached, top), cursor);
}

// comp_botline と set_empty_rows: 先頭から窓に全部入る行を数える。
[[nodiscard]] Shown shown_from(const VimToplineWindow &window, std::size_t count,
                               std::size_t top) noexcept
{
    Rows done = 0;
    std::size_t line = top;
    while (line <= count)
    {
        const Rows rows = plines(window, count, line);
        if (done + rows > height_of(window))
        {
            break;
        }
        done += rows;
        ++line;
    }
    return Shown{top, line, done == 0 ? 0 : height_of(window) - done};
}

// check_botline: キャレットが窓の下にあるか、下に余白の画面行が無いか。窓の下の行が本文の外
// （末尾まで見えている）なら要らない。
[[nodiscard]] bool bottom_short(const VimToplineWindow &window, std::size_t count,
                                const Shown &shown, std::size_t cursor) noexcept
{
    const std::size_t botline = std::get<1>(shown);
    if (botline > count)
    {
        return false;
    }
    if (cursor >= botline)
    {
        return true;
    }
    const Rows margin = margin_of(window);
    if (cursor + static_cast<std::size_t>(margin) < botline)
    {
        return false;
    }
    Rows below = std::get<2>(shown);
    for (std::size_t line = cursor + 1; line < botline && below < margin; ++line)
    {
        below += plines(window, count, line);
    }
    return below < margin;
}

// 窓の下の行から下の段（scroll_cursor_bot の scrolled・窓の下の行は空き段を引く）。
[[nodiscard]] Rows past_window(std::size_t line, const Shown &shown, Rows rows) noexcept
{
    if (line < std::get<1>(shown))
    {
        return 0;
    }
    return line == std::get<1>(shown) ? rows - std::get<2>(shown) : rows;
}

// scroll_cursor_bot
// の歩みを止めるか（送りと余白が足りたか本文の末尾に届き、窓の下の行まで数えた）。
[[nodiscard]] bool bottom_settled(Rows margin, const Walk &walk, std::size_t botline,
                                  std::size_t count) noexcept
{
    const auto &[above, below, used, scrolled, extra] = walk;
    const bool enough = (scrolled <= 0 || scrolled >= scroll_jump) && extra >= margin;
    return (enough || below + 1 > count) && above <= botline;
}

// scroll_cursor_bot の 1 歩: 上へ 1 行、本文の末尾の前なら下へ 1 行。窓を越えたら偽。
[[nodiscard]] bool stepped_bottom(const VimToplineWindow &window, std::size_t count,
                                  const Shown &shown, Walk &walk) noexcept
{
    auto &[above, below, used, scrolled, extra] = walk;
    const Rows upper = back_plines(window, count, above - 1);
    used += upper;
    if (used > height_of(window))
    {
        return false;
    }
    --above;
    scrolled += past_window(above, shown, upper);
    if (below >= count)
    {
        return true;
    }
    const Rows lower = plines(window, count, below + 1);
    used += lower;
    if (used > height_of(window))
    {
        return false;
    }
    ++below;
    if (extra < margin_of(window) || scrolled < scroll_jump)
    {
        extra += lower;
        scrolled += past_window(below, shown, lower);
    }
    return true;
}

// 送る論理行の数（scroll_cursor_bot の line_count・窓より大きければ使った段、窓の下の行を
// 越えて足りなければ 9999）。
[[nodiscard]] Rows lines_to_scroll(const VimToplineWindow &window, std::size_t count,
                                   const Shown &shown, const Walk &walk) noexcept
{
    const Rows scrolled = std::get<3>(walk);
    if (scrolled <= 0)
    {
        return 0;
    }
    if (std::get<2>(walk) > height_of(window))
    {
        return std::get<2>(walk);
    }
    Rows reached = 0;
    Rows lines = 0;
    for (std::size_t line = std::get<0>(shown); reached < scrolled && line <= std::get<1>(shown);
         ++line)
    {
        reached += plines(window, count, line);
        ++lines;
    }
    return reached < scrolled ? 9999 : lines;
}

// scroll_cursor_bot（set_topbot は偽）: 窓の下に余白の行が残るように送る。送りが窓の高さ以上なら
// 上を先に足す scroll_cursor_halfway。
[[nodiscard]] std::size_t cursor_bot(const VimToplineWindow &window, std::size_t count,
                                     const Shown &shown, std::size_t cursor) noexcept
{
    const Rows used = plines(window, count, cursor);
    Walk walk{cursor, cursor, used, past_window(cursor, shown, used), 0};
    while (std::get<0>(walk) > 1 &&
           !bottom_settled(margin_of(window), walk, std::get<1>(shown), count))
    {
        if (!stepped_bottom(window, count, shown, walk))
        {
            break;
        }
    }
    const Rows lines = lines_to_scroll(window, count, shown, walk);
    if (lines >= height_of(window) && lines > scroll_jump)
    {
        return cursor_halfway(window, count, cursor, true);
    }
    return std::min(std::get<0>(shown) + static_cast<std::size_t>(lines), count);
}

// update_topline の下側: check_botline が要れば、近ければ scroll_cursor_bot、遠ければ
// scroll_cursor_halfway。
[[nodiscard]] std::size_t bottom_checked(const VimToplineWindow &window, std::size_t count,
                                         std::size_t top, std::size_t cursor) noexcept
{
    const Shown shown = shown_from(window, count, top);
    if (!bottom_short(window, count, shown, cursor))
    {
        return top;
    }
    const std::size_t lines =
        cursor + 1 + static_cast<std::size_t>(margin_of(window)) - std::get<1>(shown);
    return lines <= window.height + 1 ? cursor_bot(window, count, shown, cursor)
                                      : cursor_halfway(window, count, cursor, false);
}

// update_topline の上側が要るか（先頭が 1 行目でなく、キャレットが先頭より上か上の余白が無い）。
[[nodiscard]] bool top_needed(const VimToplineWindow &window, std::size_t count, std::size_t top,
                              std::size_t cursor) noexcept
{
    return top > 1 && (cursor < top || top_offset_short(window, count, top, cursor));
}
} // namespace

LineNumber vim_topline_updated(const VimToplineWindow &window, LineNumber caret,
                               std::size_t line_count) noexcept
{
    const std::size_t count = std::max<std::size_t>(line_count, 1);
    const std::size_t cursor = std::clamp<std::size_t>(caret.value, 1, count);
    std::size_t top = std::clamp<std::size_t>(window.top.value, 1, count);
    if (window.height == 0)
    {
        return LineNumber{cursor};
    }
    if (top_needed(window, count, top, cursor))
    {
        // キャレットから先頭 + 余白までの論理行（Vim の n）が halfheight 以上なら中央へ寄せる。
        const auto distance =
            static_cast<Rows>(top) + margin_of(window) - static_cast<Rows>(cursor);
        const Rows halfheight = std::max<Rows>(height_of(window) / 2 - 1, 2);
        if (distance >= halfheight)
        {
            return LineNumber{cursor_halfway(window, count, cursor, false)};
        }
        top = cursor_top(window, count, top, cursor);
    }
    return LineNumber{bottom_checked(window, count, top, cursor)};
}

bool vim_topline_kept(const VimToplineWindow &window, LineNumber caret,
                      std::size_t line_count) noexcept
{
    const std::size_t count = std::max<std::size_t>(line_count, 1);
    const std::size_t cursor = std::clamp<std::size_t>(caret.value, 1, count);
    const std::size_t top = std::clamp<std::size_t>(window.top.value, 1, count);
    if (window.height == 0)
    {
        return cursor == top;
    }
    if (top_needed(window, count, top, cursor))
    {
        return false;
    }
    return !bottom_short(window, count, shown_from(window, count, top), cursor);
}
} // namespace nenenib::core
