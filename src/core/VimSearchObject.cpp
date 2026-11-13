#include "VimSearchObject.hpp"

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "VimCharacterBoundary.hpp"
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
// 当たりの始まりと終わり、SEARCH_END の後ろ向きは当たりの最後の文字と始まり。
using Found = std::pair<Offset, Offset>;

// 探し始め。値が無いのは本文の頭の手前（Vim の lnum 0）で、1 行目の 0 桁目の当たりも数える。
using Origin = std::optional<Offset>;

// 続けて探す向きと回数。
using Steps = std::pair<VimSearchDirection, std::size_t>;

// 2 回目の探索の起点と、パターンが幅の無い当たりか。
using Start = std::pair<Origin, bool>;

[[nodiscard]] VimMatchRequest request_at(const TextBuffer &text, Offset from, Steps steps)
{
    return VimMatchRequest{text.position_of(from), steps.first, steps.second};
}

[[nodiscard]] std::optional<Hit> hit_of(const TextBuffer &text, const VimPattern &pattern,
                                        const VimMatchRequest &request)
{
    const auto hit = vim_find_match(text, pattern, request);
    if (!hit.has_value())
    {
        return std::nullopt;
    }
    return Hit{text.offset_of(hit.value().position), hit.value().wrapped};
}

// 前の答えから続けて steps の回数ぶん（折り返しは足し合わせる）。回数が 0 なら前の答えのまま。
[[nodiscard]] std::optional<Hit> continued(const TextBuffer &text, const VimPattern &pattern,
                                           std::optional<Hit> at, Steps steps)
{
    if (!at.has_value() || steps.second == 0)
    {
        return at;
    }
    const auto next = hit_of(text, pattern, request_at(text, at.value().first, steps));
    if (!next.has_value())
    {
        return std::nullopt;
    }
    return Hit{next.value().first, next.value().second || at.value().second};
}

// begin に始まる当たりの終わり。行の走査は vim_line_matches と同じ 1 本（当たりの始まりは
// vim_find_match が同じ走査から返した位置）。
[[nodiscard]] Offset match_end(const TextBuffer &text, const VimPattern &pattern, Offset begin)
{
    const LineNumber line = text.line_containing(begin);
    const Offset start = text.line_start(line);
    for (const OffsetRange &match : vim_line_matches(text.line_text(line), pattern))
    {
        if (match.begin.value == begin.value - start.value)
        {
            return Offset{start.value + match.end.value};
        }
    }
    return begin;
}

// 当たりの最後の文字の先頭（Vim の SEARCH_END の pos・dec_cursor）。end は当たりの終わり。
[[nodiscard]] Offset last_character(const TextBuffer &text, Offset begin, Offset end)
{
    const LineNumber line = text.line_containing(begin);
    const Offset start = text.line_start(line);
    const Offset relative =
        vim_character_start(text.line_text(line), Offset{end.value - start.value});
    return Offset{start.value + relative.value};
}

// 本文の頭の手前から前向きに 1 つ（1 行目は行全体を見る・Vim の at_first_line が偽）。
[[nodiscard]] std::optional<Hit> first_from_top(const TextBuffer &text, const VimPattern &pattern)
{
    const LineNumber first{1};
    const auto own = vim_line_matches(text.line_text(first), pattern);
    if (!own.empty())
    {
        return Hit{Offset{text.line_start(first).value + own.front().begin.value}, false};
    }
    return hit_of(text, pattern,
                  request_at(text, text.line_end(first), Steps{VimSearchDirection::forward, 1}));
}

// 前向きに count 個目（Vim の searchit の FORWARD）。
[[nodiscard]] std::optional<Hit> forward_hit(const TextBuffer &text, const VimPattern &pattern,
                                             Origin from, std::size_t count)
{
    if (from.has_value())
    {
        return hit_of(text, pattern,
                      request_at(text, from.value(), Steps{VimSearchDirection::forward, count}));
    }
    return continued(text, pattern, first_from_top(text, pattern),
                     Steps{VimSearchDirection::forward, count - 1});
}

// 後ろ向きに count 個目（SEARCH_END なし・始まりが起点より前の当たり）。
[[nodiscard]] std::optional<Hit> backward_hit(const TextBuffer &text, const VimPattern &pattern,
                                              Offset from, std::size_t count)
{
    return hit_of(text, pattern,
                  request_at(text, from, Steps{VimSearchDirection::backward, count}));
}

// 後ろ向きの SEARCH_END。起点の行では終わりが起点より前の当たりだけを数える（始まりが起点より前
// でも終わりが起点を越える当たりは 1 つ手前へ）。2 つ目からは手前の当たりの始まりから数える。
[[nodiscard]] std::optional<Hit> backward_end_hit(const TextBuffer &text, const VimPattern &pattern,
                                                  Offset from, std::size_t count)
{
    auto at = backward_hit(text, pattern, from, 1);
    if (at.has_value() && !at.value().second && from < match_end(text, pattern, at.value().first))
    {
        at = continued(text, pattern, at, Steps{VimSearchDirection::backward, 1});
    }
    return continued(text, pattern, at, Steps{VimSearchDirection::backward, count - 1});
}

// 当たりから Vim の pos と end_pos へ。ends は SEARCH_END（空の当たりは始まりと終わりのまま）。
[[nodiscard]] Found found_of(const TextBuffer &text, const VimPattern &pattern, Hit hit, bool ends)
{
    const Offset begin = hit.first;
    const Offset end = match_end(text, pattern, begin);
    if (!ends || end == begin)
    {
        return Found{begin, end};
    }
    return Found{last_character(text, begin, end), begin};
}

// Vim の incl（次の文字へ。行の内容の終わり（NUL）に着いたら次の行の頭へ越える・最後の行は NUL）。
[[nodiscard]] Offset stepped_forward(const TextBuffer &text, Offset at)
{
    const LineNumber line = text.line_containing(at);
    const Offset start = text.line_start(line);
    const Offset end = text.line_end(line);
    const bool last = line.value >= text.line_count();
    if (at.value < end.value)
    {
        const Offset next{
            start.value +
            vim_character_end(text.line_text(line), Offset{at.value - start.value}).value};
        if (next.value < end.value || last)
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
    if (start.value < at.value)
    {
        return last_character(text, start, at);
    }
    if (line.value <= 1)
    {
        return at;
    }
    const LineNumber above{line.value - 1};
    const Offset above_start = text.line_start(above);
    const Offset above_end = text.line_end(above);
    return above_end == above_start ? above_end : last_character(text, above_start, above_end);
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

// パターンが幅の無い当たりか（Vim の is_zero_width）。本文の頭から最初の当たりが空かで決める。
// どこにも当たらなければ値が無い。
[[nodiscard]] std::optional<bool> zero_width(const TextBuffer &text, const VimPattern &pattern)
{
    const auto first = forward_hit(text, pattern, std::nullopt, 1);
    if (!first.has_value())
    {
        return std::nullopt;
    }
    return match_end(text, pattern, first.value().first) == first.value().first;
}

// 1 回目（折り返さない・回数 1）。`gn` は後ろ向き（幅のあるパターンは SEARCH_END）、`gN` は
// 前向き。見つからなければ前向きは本文の頭の手前・後ろ向きは本文の末尾から探し直す。
[[nodiscard]] Origin first_round(const TextBuffer &text, const VimPattern &pattern,
                                 const VimSearchObjectRequest &request, bool zero)
{
    const bool forward = is_forward(request);
    const Offset pos = start_of(text, request);
    if (skips_first_backward(request))
    {
        return pos;
    }
    const bool ends = forward && !zero;
    const std::optional<Hit> hit = !forward ? forward_hit(text, pattern, pos, 1)
                                   : ends   ? backward_end_hit(text, pattern, pos, 1)
                                            : backward_hit(text, pattern, pos, 1);
    if (!hit.has_value() || hit.value().second)
    {
        return forward ? Origin{} : Origin{text.line_end(LineNumber{text.line_count()})};
    }
    return found_of(text, pattern, hit.value(), ends).first;
}

// 2 回目（折り返す・回数ぶん）。`gn` は前向き、`gN` は後ろ向き（幅のあるパターンは SEARCH_END）。
[[nodiscard]] std::optional<Found> second_round(const TextBuffer &text, const VimPattern &pattern,
                                                Start start, const VimSearchObjectRequest &request)
{
    const bool ends = !is_forward(request) && !start.second;
    const Offset from = start.first.value_or(Offset{0});
    const std::optional<Hit> hit = is_forward(request)
                                       ? forward_hit(text, pattern, start.first, request.count)
                                   : ends ? backward_end_hit(text, pattern, from, request.count)
                                          : backward_hit(text, pattern, from, request.count);
    if (!hit.has_value())
    {
        return std::nullopt;
    }
    return found_of(text, pattern, hit.value(), ends);
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
        return Selection{anchor, skips_first_backward(request)
                                     ? start_pos
                                     : last_character(text, start_pos, end_pos)};
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
    const auto zero = zero_width(text, pattern);
    if (!zero.has_value())
    {
        return std::unexpected(VimSearchNoticeKind::pattern_not_found);
    }
    const Origin pos = first_round(text, pattern, request, zero.value());
    const auto found = second_round(text, pattern, Start{pos, zero.value()}, request);
    if (!found.has_value())
    {
        return std::unexpected(VimSearchNoticeKind::pattern_not_found);
    }
    return placed(text, request, found.value());
}
} // namespace nenenib::core
