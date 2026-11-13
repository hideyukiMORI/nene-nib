#include "VimSearchObject.hpp"

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "VimCaret.hpp"
#include "VimMatchRequest.hpp"
#include "VimSearch.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchHit.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <utility>

namespace nenenib::core
{
namespace
{
// 当たりの始まりと、本文の端を越えて折り返したか。
using Hit = std::pair<Offset, bool>;

// Vim の searchit が置く 2 つの位置（pos と end_pos）。前向きと SEARCH_END なしの後ろ向きは
// 当たりの始まりと終わり、SEARCH_END の後ろ向きは当たりの最後の文字と始まり（空の当たりは
// 始まりと始まり）。
using Found = std::pair<Offset, Offset>;

// 見つけた位置と、どこかで折り返したか。
using Landed = std::pair<Found, bool>;

// 続けて探す向きと回数。
using Steps = std::pair<VimSearchDirection, std::size_t>;

// 探し方: 向きと回数と、SEARCH_END か。
using Way = std::pair<Steps, bool>;

// 探し始め。値が無いのは本文の頭の手前（Vim の lnum 0）で、1 行目の 0 桁目の当たりも数える。
using Origin = std::optional<Offset>;

// 2 回目の探索の起点と、パターンが幅の無い当たりか。
using Start = std::pair<Origin, bool>;

// 費用（M4・#1617）: 行を写さず、当たりを全部数えない。探すのは vim_find_match（`n` `N` と同じ
// 1 回ぶんの走査）と、後ろ向きの SEARCH_END の起点の行だけ vim_last_match_ending_by（同じ走査を
// 1 回）。当たりの終わりは vim_match_from の 1 回の照合、文字の境は vim_character_after /
// vim_character_before の窓で読む。1 回の `gn` `gN` は Vim と同じく行の走査 2 回ぶん。

[[nodiscard]] std::optional<Hit> hit_of(const TextBuffer &text, const VimPattern &pattern,
                                        Offset from, Steps steps)
{
    const auto hit = vim_find_match(
        text, pattern, VimMatchRequest{text.position_of(from), steps.first, steps.second});
    if (!hit.has_value())
    {
        return std::nullopt;
    }
    return Hit{text.offset_of(hit.value().position), hit.value().wrapped};
}

// begin に始まる当たりの終わり（begin は vim_find_match が返した区切りの当たりの始まり）。
[[nodiscard]] Offset match_end(const TextBuffer &text, const VimPattern &pattern, Offset begin)
{
    const auto match = vim_match_from(text, pattern, begin);
    return match.has_value() && match.value().begin == begin ? match.value().end : begin;
}

// 当たりの最後の文字の先頭（Vim の SEARCH_END の pos と dec_cursor）。空の当たりは始まりのまま。
[[nodiscard]] Offset last_character(const TextBuffer &text, Offset begin, Offset end)
{
    if (!(begin < end))
    {
        return end;
    }
    return vim_character_before(text, end, text.line_start(text.line_containing(begin)));
}

// Vim の
// dec_cursor（前の文字へ。行の頭からは前の行の内容の終わり（NUL）へ・本文の頭では動かない）。
[[nodiscard]] Offset decremented(const TextBuffer &text, Offset at)
{
    const LineNumber line = text.line_containing(at);
    const Offset start = text.line_start(line);
    if (start < at)
    {
        return vim_character_before(text, at, start);
    }
    return line.value > 1 ? text.line_end(LineNumber{line.value - 1}) : at;
}

// 本文の頭の手前から前向きに 1 つ（1 行目は 0 桁目から見る・Vim の at_first_line が偽）。
[[nodiscard]] std::optional<Hit> first_from_top(const TextBuffer &text, const VimPattern &pattern)
{
    const LineNumber first{1};
    const auto own = vim_match_from(text, pattern, text.line_start(first));
    if (own.has_value())
    {
        return Hit{own.value().begin, false};
    }
    return hit_of(text, pattern, text.line_end(first), Steps{VimSearchDirection::forward, 1});
}

// 前向きに count 個目（Vim の searchit の FORWARD）。
[[nodiscard]] std::optional<Hit> forward_hit(const TextBuffer &text, const VimPattern &pattern,
                                             Origin from, std::size_t count)
{
    if (from.has_value())
    {
        return hit_of(text, pattern, from.value(), Steps{VimSearchDirection::forward, count});
    }
    const auto top = first_from_top(text, pattern);
    if (!top.has_value() || count <= 1)
    {
        return top;
    }
    const auto next =
        hit_of(text, pattern, top.value().first, Steps{VimSearchDirection::forward, count - 1});
    if (!next.has_value())
    {
        return std::nullopt;
    }
    return Hit{next.value().first, next.value().second || top.value().second};
}

// 後ろ向きの SEARCH_END の 1 回（Vim の searchit の BACKWARD と SEARCH_END）。起点の行では終わりが
// 起点を越えない区切りの当たりの最後（終わり − 1 < 起点の桁・起点ちょうどの空の当たりも入る・
// vim_last_match_ending_by の 1 回の走査）。起点が行頭なら起点の行を見ない（start_pos.col == 0）。
// 起点の行に無ければ上の行から（ほかの行は行の最後の当たり・vim_find_match の後ろ向き）。
[[nodiscard]] std::optional<Hit> backward_end_once(const TextBuffer &text,
                                                   const VimPattern &pattern, Offset from)
{
    const Offset line_start = text.line_start(text.line_containing(from));
    if (from != line_start)
    {
        const auto own = vim_last_match_ending_by(text, pattern, from);
        if (own.has_value())
        {
            return Hit{own.value().begin, false};
        }
    }
    return hit_of(text, pattern, line_start, Steps{VimSearchDirection::backward, 1});
}

// 当たりから Vim の pos と end_pos へ。ends は SEARCH_END（空の当たりは始まりと始まり）。
[[nodiscard]] Found found_of(const TextBuffer &text, const VimPattern &pattern, Offset begin,
                             bool ends)
{
    const Offset end = match_end(text, pattern, begin);
    if (!ends)
    {
        return Found{begin, end};
    }
    return Found{last_character(text, begin, end), begin};
}

// 折り返しで最初の答えへ戻ったら、答えの列は step 歩ごとの繰り返しなので残りの回数から繰り返しの
// 分を飛ばす（巨大な回数の `gN` が回り続けない・vim_find_match の #1334 と同じ形）。1 歩の答えは
// 起点だけで決まる。
[[nodiscard]] std::size_t cycled_step(std::optional<Offset> &first, Offset at, std::size_t step,
                                      std::size_t count)
{
    if (!first.has_value())
    {
        first = at;
        return step;
    }
    if (!(first.value() == at))
    {
        return step;
    }
    const std::size_t remaining = count - 1U - step;
    return step + (remaining / step) * step;
}

// 後ろ向きの SEARCH_END を count 回（2 回目からは前の答えの pos = 最後の文字から・Vim の
// searchit の回数の繰り返し）。
[[nodiscard]] std::optional<Landed> backward_ends(const TextBuffer &text, const VimPattern &pattern,
                                                  Offset from, std::size_t count)
{
    Found found{from, from};
    bool wrapped = false;
    std::optional<Offset> first;
    for (std::size_t step = 0; step < count; ++step)
    {
        const auto hit = backward_end_once(text, pattern, found.first);
        if (!hit.has_value())
        {
            return std::nullopt;
        }
        found = found_of(text, pattern, hit.value().first, true);
        wrapped = wrapped || hit.value().second;
        step = cycled_step(first, found.first, step, count);
    }
    return Landed{found, wrapped};
}

// 1 回の探索（ends は SEARCH_END）。
[[nodiscard]] std::optional<Landed> searched(const TextBuffer &text, const VimPattern &pattern,
                                             Origin from, Way way)
{
    const auto [steps, ends] = way;
    const auto [direction, count] = steps;
    if (direction == VimSearchDirection::backward && ends)
    {
        return backward_ends(text, pattern, from.value_or(Offset{0}), count);
    }
    const auto hit = direction == VimSearchDirection::forward
                         ? forward_hit(text, pattern, from, count)
                         : hit_of(text, pattern, from.value_or(Offset{0}), steps);
    if (!hit.has_value())
    {
        return std::nullopt;
    }
    return Landed{found_of(text, pattern, hit.value().first, false), hit.value().second};
}

// Vim の incl（次の文字へ。行の内容の終わり（NUL）に着いたら次の行の頭へ越える・最後の行は NUL）。
[[nodiscard]] Offset stepped_forward(const TextBuffer &text, Offset at)
{
    const LineNumber line = text.line_containing(at);
    const Offset end = text.line_end(line);
    const bool last = line.value >= text.line_count();
    if (at < end)
    {
        const Offset next = vim_character_after(text, at, end);
        if (next < end || last)
        {
            return next;
        }
    }
    return last ? at : text.line_start(LineNumber{line.value + 1});
}

// Vim の decl（前の文字へ。行の頭からは前の行の最後の文字へ・空の行ならその NUL）。
[[nodiscard]] Offset stepped_backward(const TextBuffer &text, Offset at)
{
    const LineNumber line = text.line_containing(at);
    const Offset start = text.line_start(line);
    if (start < at)
    {
        return vim_character_before(text, at, start);
    }
    if (line.value <= 1)
    {
        return at;
    }
    const LineNumber above{line.value - 1};
    const Offset above_start = text.line_start(above);
    const Offset above_end = text.line_end(above);
    return above_end == above_start ? above_end
                                    : vim_character_before(text, above_end, above_start);
}

[[nodiscard]] bool is_forward(const VimSearchObjectRequest &request) noexcept
{
    return request.direction == VimSearchDirection::forward;
}

[[nodiscard]] bool is_visual(const VimSearchObjectRequest &request) noexcept
{
    return request.from == VimSearchObjectFrom::selection;
}

// 前向きで VISUAL の選択が逆向き（キャレットが anchor より前）なら 1 回目の後ろ向きを飛ばす
// （Vim の skip_first_backward）。
[[nodiscard]] bool skips_first_backward(const VimSearchObjectRequest &request) noexcept
{
    return is_forward(request) && is_visual(request) &&
           request.selection.caret < request.selection.anchor;
}

// 探し始め。VISUAL は 1 文字先（`gN` は手前）へずらす（Vim の incl / decl）。
[[nodiscard]] Offset start_of(const TextBuffer &text, const VimSearchObjectRequest &request)
{
    const Offset caret = request.selection.caret;
    if (!is_visual(request))
    {
        return caret;
    }
    return is_forward(request) ? stepped_forward(text, caret) : stepped_backward(text, caret);
}

// 本文の頭から前向きに探した最初の当たり（Vim の is_zero_width の move が真の形）。どこにも
// 当たらなければ値が無い。これが空ならパターンは幅の無い当たり（キャレットの位置の当たりでは
// 決めない）。
[[nodiscard]] std::optional<OffsetRange> top_match(const TextBuffer &text,
                                                   const VimPattern &pattern)
{
    const auto first = first_from_top(text, pattern);
    if (!first.has_value())
    {
        return std::nullopt;
    }
    const Offset begin = first.value().first;
    return OffsetRange{begin, match_end(text, pattern, begin)};
}

[[nodiscard]] bool empty_range(const OffsetRange &range) noexcept
{
    return range.begin == range.end;
}

// `gn` の 1 回目（後ろ向き・折り返さない）に答えがあるか。本文の最初の当たり top より前に当たりは
// 無いので、SEARCH_END（top は空でない）なら top の終わりが起点を越えないとき、SEARCH_END でない
// なら top の始まりが起点より前のときだけ答えがある。答えが無いと分かれば探さない（vim_find_match
// は折り返して本文の残りを歩いてしまう・#1617 のレビュー M4）。
[[nodiscard]] bool reachable_before(Offset pos, const OffsetRange &top) noexcept
{
    return empty_range(top) ? top.begin < pos : !(pos < top.end);
}

// 1 回目（折り返さない・回数 1）。`gn` は後ろ向き（幅のあるパターンは SEARCH_END）、`gN` は
// 前向き。見つからなければ前向きは本文の頭の手前・後ろ向きは本文の末尾から探し直す。
[[nodiscard]] Origin first_round(const TextBuffer &text, const VimPattern &pattern,
                                 const VimSearchObjectRequest &request, const OffsetRange &top)
{
    const bool forward = is_forward(request);
    const Offset pos = start_of(text, request);
    if (skips_first_backward(request))
    {
        return pos;
    }
    if (forward && !reachable_before(pos, top))
    {
        return Origin{};
    }
    const bool zero = empty_range(top);
    const auto landed =
        searched(text, pattern, pos,
                 Way{Steps{forward ? VimSearchDirection::backward : VimSearchDirection::forward, 1},
                     forward && !zero});
    if (!landed.has_value() || landed.value().second)
    {
        return forward ? Origin{} : Origin{text.line_end(LineNumber{text.line_count()})};
    }
    return landed.value().first.first;
}

// 2 回目（折り返す・回数ぶん）。`gn` は前向き、`gN` は後ろ向き（幅のあるパターンは SEARCH_END）。
[[nodiscard]] std::optional<Found> second_round(const TextBuffer &text, const VimPattern &pattern,
                                                Start start, const VimSearchObjectRequest &request)
{
    const auto landed = searched(
        text, pattern, start.first,
        Way{Steps{request.direction, request.count}, !is_forward(request) && !start.second});
    if (!landed.has_value())
    {
        return std::nullopt;
    }
    return landed.value().first;
}

// 見つけた当たりの置き方（current_search の最後）。NORMAL は anchor を当たりの始まりに置き、
// 前向きはキャレットを最後の文字へ（選択が逆向きなら始まりへ）、後ろ向きは始まりへ。
[[nodiscard]] Selection placed(const TextBuffer &text, const VimSearchObjectRequest &request,
                               Found found)
{
    const auto [start_pos, end_pos] = found;
    const Offset anchor = is_visual(request) ? request.selection.anchor : start_pos;
    if (is_forward(request) && anchor < end_pos)
    {
        return Selection{anchor,
                         skips_first_backward(request) ? start_pos : decremented(text, end_pos)};
    }
    if (is_forward(request) && is_visual(request) && end_pos < anchor)
    {
        return Selection{anchor, start_pos};
    }
    return Selection{anchor, end_pos};
}
} // namespace

std::expected<Selection, VimSearchNoticeKind>
vim_search_object(const TextBuffer &text, const VimPattern &pattern,
                  const VimSearchObjectRequest &request)
{
    const auto top = top_match(text, pattern);
    if (!top.has_value())
    {
        return std::unexpected(VimSearchNoticeKind::pattern_not_found);
    }
    const Origin pos = first_round(text, pattern, request, top.value());
    const auto found = second_round(text, pattern, Start{pos, empty_range(top.value())}, request);
    if (!found.has_value())
    {
        return std::unexpected(VimSearchNoticeKind::pattern_not_found);
    }
    return placed(text, request, found.value());
}
} // namespace nenenib::core
