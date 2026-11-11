#include "VimToplineUpdate.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <optional>
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
// 読む窓・行数・載っていなくて最初に読んだ行（問いとして返す）。
using Lookup = std::tuple<const VimToplineWindow &, std::size_t, std::optional<std::size_t>>;
// 答えの先頭と、送った論理行（scroll_cursor_bot の scrollup の line_count・送らなければ 0）。
using Scrolled = std::pair<std::size_t, std::size_t>;
// update_topline の 1 回の答え: 先頭・Vim の w_botline を数えた窓の先頭・その後に scrollup が
// w_botline へ足した行（win_update が描いた窓の下の行と比べる値は、この 2 つから決まる）。
using Settled = std::tuple<std::size_t, std::size_t, std::size_t>;

[[nodiscard]] Rows height_of(const Lookup &lookup) noexcept
{
    return static_cast<Rows>(std::get<0>(lookup).height);
}

[[nodiscard]] Rows margin_of(const Lookup &lookup) noexcept
{
    return static_cast<Rows>(std::min(std::get<0>(lookup).margin, margin_limit));
}

[[nodiscard]] std::size_t count_of(const Lookup &lookup) noexcept
{
    return std::get<1>(lookup);
}

// Vim の plines。本文の外は beyond。載っていない行は 1 と数え、問う道なら最初の 1 行を覚える（その
// 回の答えは捨てて、application が測ってから呼び直す）。
[[nodiscard]] Rows plines(Lookup &lookup, std::size_t line) noexcept
{
    auto &[window, count, wanted] = lookup;
    if (line < 1 || line > count)
    {
        return beyond;
    }
    const auto found = std::ranges::lower_bound(window.rows, line, {}, [](const VimLineRows &value)
                                                { return value.line.value; });
    if (found != window.rows.end() && found->line.value == line)
    {
        return static_cast<Rows>(std::max<std::size_t>(found->rows, 1));
    }
    if (window.asks && !wanted.has_value())
    {
        wanted = line;
    }
    return 1;
}

// topline_back の高さ（Vim の PLINES_WIN_NOFILL は窓の高さで切る・本文の上は beyond）。
[[nodiscard]] Rows back_plines(Lookup &lookup, std::size_t line) noexcept
{
    const Rows rows = plines(lookup, line);
    return line < 1 ? rows : std::min(rows, std::max<Rows>(height_of(lookup), 1));
}

// check_top_offset: キャレットの上に余白の画面行が窓の中に無いか。
[[nodiscard]] bool top_offset_short(Lookup &lookup, std::size_t top, std::size_t cursor) noexcept
{
    const Rows margin = margin_of(lookup);
    if (cursor >= top + static_cast<std::size_t>(margin))
    {
        return false;
    }
    Rows above = 0;
    for (std::size_t line = cursor; above < margin && line > top; --line)
    {
        above += back_plines(lookup, line - 1);
    }
    return above < margin;
}

// scroll_cursor_halfway の下へ 1 行（本文の下は「~」の 1 段で、使った段は増やさない）。
[[nodiscard]] bool added_below(Lookup &lookup, Side &below, Rows &used) noexcept
{
    if (below.first >= count_of(lookup))
    {
        ++below.second;
        return true;
    }
    const Rows rows = plines(lookup, below.first + 1);
    used += rows;
    if (used > height_of(lookup))
    {
        return false;
    }
    ++below.first;
    below.second += rows;
    return true;
}

[[nodiscard]] bool added_above(Lookup &lookup, Side &above, Rows &used) noexcept
{
    const Rows rows = back_plines(lookup, above.first - 1);
    used += rows;
    if (used > height_of(lookup))
    {
        return false;
    }
    --above.first;
    above.second += rows;
    return true;
}

// 1 周で下を先に足す（Vim の prefer_above が偽: 下が上以下なら下、その後に下が上より多ければ上）。
[[nodiscard]] bool balanced_below_first(Lookup &lookup, Halves &halves, Rows &used) noexcept
{
    auto &[above, below] = halves;
    if (below.second <= above.second && !added_below(lookup, below, used))
    {
        return false;
    }
    return below.second <= above.second || added_above(lookup, above, used);
}

// 1 周で上を先に足す（prefer_above が真: 下が上以上なら上、その後に下が上より少なければ下）。
[[nodiscard]] bool balanced_above_first(Lookup &lookup, Halves &halves, Rows &used) noexcept
{
    auto &[above, below] = halves;
    if (below.second >= above.second && !added_above(lookup, above, used))
    {
        return false;
    }
    return below.second >= above.second || added_below(lookup, below, used);
}

// scroll_cursor_halfway（atend は偽）: キャレットの行から上と下へ交互に足し、窓を越える手前の
// 上の行を先頭にする。
[[nodiscard]] std::size_t cursor_halfway(Lookup &lookup, std::size_t cursor,
                                         bool prefer_above) noexcept
{
    Halves halves{Side{cursor, 0}, Side{cursor, 0}};
    Rows used = plines(lookup, cursor);
    while (halves.first.first > 1)
    {
        const bool going = prefer_above ? balanced_above_first(lookup, halves, used)
                                        : balanced_below_first(lookup, halves, used);
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
[[nodiscard]] Rows paired_below(Lookup &lookup, std::size_t line, bool wanted) noexcept
{
    return wanted && line < count_of(lookup) ? plines(lookup, line) : 0;
}

// 上の行を足すのを止めるか（送りが scrolljump を越えたか先頭に届き、上の余白が足りた）。
[[nodiscard]] bool top_settled(bool reached, Rows scrolled, Rows extra, Rows margin) noexcept
{
    return (reached || scrolled > scroll_jump) && extra >= margin;
}

// scroll_cursor_top（always は偽）: キャレットの上に余白の行を置いた先頭。窓に収まらなければ
// scroll_cursor_halfway。先頭は下げない（折り返す行で上がるだけ）。
[[nodiscard]] std::size_t cursor_top(Lookup &lookup, std::size_t top, std::size_t cursor) noexcept
{
    const Rows margin = margin_of(lookup);
    Rows used = plines(lookup, cursor);
    Rows scrolled = above_window(cursor, top, used);
    Rows extra = 0;
    std::size_t above = cursor - 1;
    std::size_t below = cursor + 1;
    std::size_t reached = cursor;
    while (above > 0)
    {
        const Rows rows = plines(lookup, above);
        scrolled += above_window(above, top, rows);
        if (top_settled(reached >= top, scrolled, extra, margin))
        {
            break;
        }
        used += rows + paired_below(lookup, below, extra + rows <= margin);
        if (used > height_of(lookup))
        {
            break;
        }
        extra += rows;
        reached = above;
        --above;
        ++below;
    }
    if (used > height_of(lookup))
    {
        return cursor_halfway(lookup, cursor, false);
    }
    return std::min(std::min(reached, top), cursor);
}

// comp_botline と set_empty_rows: 先頭から窓に全部入る行を数える。
[[nodiscard]] Shown shown_from(Lookup &lookup, std::size_t top) noexcept
{
    Rows done = 0;
    std::size_t line = top;
    while (line <= count_of(lookup))
    {
        const Rows rows = plines(lookup, line);
        if (done + rows > height_of(lookup))
        {
            break;
        }
        done += rows;
        ++line;
    }
    return Shown{top, line, done == 0 ? 0 : height_of(lookup) - done};
}

// check_botline: キャレットが窓の下にあるか、下に余白の画面行が無いか。窓の下の行が本文の外
// （末尾まで見えている）なら要らない。
[[nodiscard]] bool bottom_short(Lookup &lookup, const Shown &shown, std::size_t cursor) noexcept
{
    const std::size_t botline = std::get<1>(shown);
    if (botline > count_of(lookup))
    {
        return false;
    }
    if (cursor >= botline)
    {
        return true;
    }
    const Rows margin = margin_of(lookup);
    if (cursor + static_cast<std::size_t>(margin) < botline)
    {
        return false;
    }
    Rows below = std::get<2>(shown);
    for (std::size_t line = cursor + 1; line < botline && below < margin; ++line)
    {
        below += plines(lookup, line);
    }
    return below < margin;
}

// INSERT で打った本文を 1 文字ずつ下へ歩いたキャレットの先頭か（#1363）。Vim の edit は 1 文字
// ごとに update_topline を呼ぶので、キャレットは窓の下の行を 1 行ずつ越え、そのたびに
// scroll_cursor_bot が窓を最小の行だけ送る。余白が窓の半分より小さければ、送りの後の先頭は
// 「キャレットと下の余白の行（本文の末尾で切る）が窓に入る最小の先頭」（scroll_cursor_bot の
// set_topbot と同じ形）で、何行歩いても同じになる。余白が窓の半分以上なら上側の余白と組み合って
// 中央へ寄るので、1 回の追従と同じ scroll_cursor_halfway のまま。
[[nodiscard]] bool walked(const Lookup &lookup) noexcept
{
    return std::get<0>(lookup).walks && margin_of(lookup) * 2 < height_of(lookup);
}

// walked の先頭: キャレットの行と下の余白の行を数えてから、窓に入るだけ上の行を足す。
[[nodiscard]] std::size_t walked_bottom(Lookup &lookup, std::size_t cursor) noexcept
{
    Rows used = plines(lookup, cursor);
    const std::size_t last =
        std::min(cursor + static_cast<std::size_t>(margin_of(lookup)), count_of(lookup));
    for (std::size_t line = cursor + 1; line <= last; ++line)
    {
        used += plines(lookup, line);
    }
    std::size_t top = cursor;
    while (top > 1)
    {
        const Rows rows = back_plines(lookup, top - 1);
        if (used + rows > height_of(lookup))
        {
            break;
        }
        used += rows;
        --top;
    }
    return top;
}

// walked の先頭と、歩くあいだの scrollup が w_botline に足した行（#1300）。Vim は 1 文字ごとの
// scroll_cursor_bot の scrollup(line_count) で w_botline += line_count とするので、歩き終わった
// ときに持っている値は「数えた窓の w_botline ＋ 送った論理行の和」で、送った論理行の和は先頭の
// 差になる。折り返さない窓ではこれが描いた w_botline と同じなので 2 回目は走らない。
[[nodiscard]] Scrolled walked_scrolled(Lookup &lookup, std::size_t top, std::size_t cursor) noexcept
{
    const std::size_t first = walked_bottom(lookup, cursor);
    return Scrolled{first, first > top ? first - top : 0};
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
[[nodiscard]] bool stepped_bottom(Lookup &lookup, const Shown &shown, Walk &walk) noexcept
{
    auto &[above, below, used, scrolled, extra] = walk;
    const Rows upper = back_plines(lookup, above - 1);
    used += upper;
    if (used > height_of(lookup))
    {
        return false;
    }
    --above;
    scrolled += past_window(above, shown, upper);
    if (below >= count_of(lookup))
    {
        return true;
    }
    const Rows lower = plines(lookup, below + 1);
    used += lower;
    if (used > height_of(lookup))
    {
        return false;
    }
    ++below;
    if (extra < margin_of(lookup) || scrolled < scroll_jump)
    {
        extra += lower;
        scrolled += past_window(below, shown, lower);
    }
    return true;
}

// 送る論理行の数（scroll_cursor_bot の line_count・窓より大きければ使った段、窓の下の行を
// 越えて足りなければ 9999）。
[[nodiscard]] Rows lines_to_scroll(Lookup &lookup, const Shown &shown, const Walk &walk) noexcept
{
    const Rows scrolled = std::get<3>(walk);
    if (scrolled <= 0)
    {
        return 0;
    }
    if (std::get<2>(walk) > height_of(lookup))
    {
        return std::get<2>(walk);
    }
    Rows reached = 0;
    Rows lines = 0;
    for (std::size_t line = std::get<0>(shown); reached < scrolled && line <= std::get<1>(shown);
         ++line)
    {
        reached += plines(lookup, line);
        ++lines;
    }
    return reached < scrolled ? 9999 : lines;
}

// scroll_cursor_bot（set_topbot は偽）: 窓の下に余白の行が残るように送る。送りが窓の高さ以上なら
// 上を先に足す scroll_cursor_halfway。
[[nodiscard]] Scrolled cursor_bot(Lookup &lookup, const Shown &shown, std::size_t cursor) noexcept
{
    const Rows used = plines(lookup, cursor);
    Walk walk{cursor, cursor, used, past_window(cursor, shown, used), 0};
    while (std::get<0>(walk) > 1 &&
           !bottom_settled(margin_of(lookup), walk, std::get<1>(shown), count_of(lookup)))
    {
        if (!stepped_bottom(lookup, shown, walk))
        {
            break;
        }
    }
    const Rows lines = lines_to_scroll(lookup, shown, walk);
    if (lines >= height_of(lookup) && lines > scroll_jump)
    {
        return walked(lookup) ? walked_scrolled(lookup, std::get<0>(shown), cursor)
                              : Scrolled{cursor_halfway(lookup, cursor, true), 0};
    }
    const auto sent = static_cast<std::size_t>(lines);
    return Scrolled{std::min(std::get<0>(shown) + sent, count_of(lookup)), sent};
}

// 先頭から行 last（本文の末尾で切る）までが窓に全部入るか。入れば w_botline は last より下で、
// キャレット + 余白 = last なら check_botline
// は要らない（窓の下の行を読まずに分かる・答えは同じ）。
[[nodiscard]] bool fits_through(Lookup &lookup, std::size_t top, std::size_t last) noexcept
{
    Rows done = 0;
    for (std::size_t line = top; line <= std::min(last, count_of(lookup)); ++line)
    {
        done += plines(lookup, line);
        if (done > height_of(lookup))
        {
            return false;
        }
    }
    return true;
}

// update_topline の下側: check_botline が要れば、近ければ scroll_cursor_bot、遠ければ
// scroll_cursor_halfway。
[[nodiscard]] Scrolled bottom_checked(Lookup &lookup, std::size_t top, std::size_t cursor) noexcept
{
    if (fits_through(lookup, top, cursor + static_cast<std::size_t>(margin_of(lookup))))
    {
        return Scrolled{top, 0};
    }
    const Shown shown = shown_from(lookup, top);
    if (!bottom_short(lookup, shown, cursor))
    {
        return Scrolled{top, 0};
    }
    const std::size_t lines =
        cursor + 1 + static_cast<std::size_t>(margin_of(lookup)) - std::get<1>(shown);
    if (lines <= std::get<0>(lookup).height + 1)
    {
        return cursor_bot(lookup, shown, cursor);
    }
    return walked(lookup) ? walked_scrolled(lookup, top, cursor)
                          : Scrolled{cursor_halfway(lookup, cursor, false), 0};
}

// update_topline の上側が要るか（先頭が 1 行目でなく、キャレットが先頭より上か上の余白が無い）。
[[nodiscard]] bool top_needed(Lookup &lookup, std::size_t top, std::size_t cursor) noexcept
{
    return top > 1 && (cursor < top || top_offset_short(lookup, top, cursor));
}

// キャレットから先頭 + 余白までの論理行（Vim の n）が halfheight 以上か。
[[nodiscard]] bool far_from_top(const Lookup &lookup, std::size_t top, std::size_t cursor) noexcept
{
    const auto distance = static_cast<Rows>(top) + margin_of(lookup) - static_cast<Rows>(cursor);
    return distance >= std::max<Rows>(height_of(lookup) / 2 - 1, 2);
}

// check_top_offset を歩かずに答えが scroll_cursor_halfway（下を先に足す）に決まるか（#1214）。
// 余白が窓の高さより大きく、先頭が 1 行目でなく、キャレットが先頭以上で n が halfheight 以上
// （このときキャレットは先頭 + 余白より上）。check_top_offset が真なら上側が halfway に寄せる。
// 偽なら先頭からキャレットの前までの画面行は余白以上 = 窓の高さ + 1 以上なので、w_botline は
// キャレットより上で check_botline が要り、Vim の line_count（キャレット − w_botline + 1 +
// 余白）は余白 + 2 以上 = 窓の高さ + 3 以上で、下側も halfway に寄せる。
[[nodiscard]] bool halfway_either_way(const Lookup &lookup, std::size_t top,
                                      std::size_t cursor) noexcept
{
    return margin_of(lookup) > height_of(lookup) && top > 1 && cursor >= top &&
           far_from_top(lookup, top, cursor);
}

// update_topline の全体（上側の後に下側・上側が中央へ寄せたら下側は見ない）。中央へ寄せる
// scroll_cursor_halfway は w_botline を数え直さない（鍵の前の窓の値が残る）。scroll_cursor_top の
// 後の check_botline は動いた窓で数え直し、scroll_cursor_bot の scrollup は送った行を足す。
[[nodiscard]] Settled updated_top(Lookup &lookup, std::size_t top, std::size_t cursor) noexcept
{
    if (halfway_either_way(lookup, top, cursor))
    {
        return Settled{cursor_halfway(lookup, cursor, false), top, 0};
    }
    if (top_needed(lookup, top, cursor))
    {
        if (far_from_top(lookup, top, cursor))
        {
            return Settled{cursor_halfway(lookup, cursor, false), top, 0};
        }
        top = cursor_top(lookup, top, cursor);
    }
    const auto [first, sent] = bottom_checked(lookup, top, cursor);
    return Settled{first, top, sent};
}

// win_update の終わりの確かめ（Vim 9.1 の drawscreen.c・dollar_vcol が -1 のとき）:
// 答えの窓を描いた w_botline が update_topline の終わりに持っていた値と違えば、update_topline
// をもう 1 回だけ呼ぶ （recursive で 1 回）。持っていた値は数えた窓の w_botline に scrollup
// の行を足したもの（本文の 末尾 + 1
// で切る）。答えが数えた窓のままで送っていなければ同じ値なので数えない。
[[nodiscard]] bool redraw_rechecks(Lookup &lookup, const Settled &settled) noexcept
{
    const auto [first, counted, sent] = settled;
    if (first == counted && sent == 0)
    {
        return false;
    }
    const std::size_t held =
        std::min(std::get<1>(shown_from(lookup, counted)) + sent, count_of(lookup) + 1);
    return std::get<1>(shown_from(lookup, first)) != held;
}
} // namespace

namespace
{
// 2 つの入口の共通（redrawn が真なら win_update の確かめも流す）。
[[nodiscard]] std::expected<LineNumber, VimRowsNeeded> topline_of(const VimToplineWindow &window,
                                                                  LineNumber caret,
                                                                  std::size_t line_count,
                                                                  bool redrawn) noexcept
{
    const std::size_t count = std::max<std::size_t>(line_count, 1);
    const std::size_t cursor = std::clamp<std::size_t>(caret.value, 1, count);
    // 先頭は本文の行数で切らない。行単位の削除の後は先頭が本文の外のまま残り、Vim の n（先頭 +
    // 余白 - キャレット）はその値で数える（Vim の mark_adjust は curwin の w_topline を動かさない・
    // #1230）。キャレットは本文の中なので、先頭が本文の外なら上側は必ず決め直す。
    const std::size_t top = std::max<std::size_t>(window.top.value, 1);
    if (window.height == 0)
    {
        return LineNumber{cursor};
    }
    Lookup lookup{window, count, std::nullopt};
    const Settled settled = updated_top(lookup, top, cursor);
    const std::size_t first = std::get<0>(settled);
    const std::size_t updated = redrawn && redraw_rechecks(lookup, settled)
                                    ? std::get<0>(updated_top(lookup, first, cursor))
                                    : first;
    const std::optional<std::size_t> &wanted = std::get<2>(lookup);
    if (wanted.has_value())
    {
        return std::unexpected(VimRowsNeeded{LineNumber{wanted.value()}});
    }
    return LineNumber{updated};
}
} // namespace

std::expected<LineNumber, VimRowsNeeded> vim_topline_updated(const VimToplineWindow &window,
                                                             LineNumber caret,
                                                             std::size_t line_count) noexcept
{
    return topline_of(window, caret, line_count, false);
}

std::expected<LineNumber, VimRowsNeeded> vim_topline_redrawn(const VimToplineWindow &window,
                                                             LineNumber caret,
                                                             std::size_t line_count) noexcept
{
    return topline_of(window, caret, line_count, true);
}
} // namespace nenenib::core
