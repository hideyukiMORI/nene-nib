#include "VimSearch.hpp"

#include "LineNumber.hpp"
#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimPatternMatch.hpp"
#include "VimSearchWalk.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
// 数え直しを始める次の位置。長さ 0 の一致は 1 文字進める（ADR 0032 の決定 4 の追記）。
[[nodiscard]] std::size_t advanced(std::string_view content, const VimPatternMatch &match)
{
    return match.end > match.begin ? match.end
                                   : next_code_point(content, Offset{match.begin}).value;
}

// window の始まりから数え、数え直す位置が window の終わりに達したら打ち切る。行の全体は
// [0, 行の長さ)。
template <typename Visitor>
void visit_matches(std::string_view line, OffsetRange window, const VimPattern &pattern,
                   const Visitor &visitor)
{
    std::size_t at = window.begin.value;
    while (true)
    {
        const auto match = pattern.matched(line, at);
        if (!match.has_value() || !visitor(match.value()))
        {
            return;
        }
        at = advanced(line, match.value());
        if (at >= window.end.value)
        {
            return;
        }
    }
}

template <typename Visitor>
void visit_line_matches(std::string_view line, const VimPattern &pattern, const Visitor &visitor)
{
    visit_matches(line, OffsetRange{Offset{0}, Offset{line.size()}}, pattern, visitor);
}

// 行の中で lower 桁以降にある最も左の一致の始まり。
[[nodiscard]] std::optional<std::size_t> first_match(std::string_view content,
                                                     const VimPattern &pattern, std::size_t lower)
{
    std::optional<std::size_t> found;
    visit_line_matches(content, pattern,
                       [&found, lower](const VimPatternMatch &match)
                       {
                           if (match.begin < lower)
                           {
                               return true;
                           }
                           found = match.begin;
                           return false;
                       });
    return found;
}

// 行の中で upper 桁より前にある最後の一致の始まり。
[[nodiscard]] std::optional<std::size_t> last_match(std::string_view content,
                                                    const VimPattern &pattern, std::size_t upper)
{
    std::optional<std::size_t> found;
    visit_line_matches(content, pattern,
                       [&found, upper](const VimPatternMatch &match)
                       {
                           if (match.begin >= upper)
                           {
                               return false;
                           }
                           found = match.begin;
                           return true;
                       });
    return found;
}

// 起点の行の前向きの一致（起点の次の桁以降）。行の内容の終わり（Vim の NUL）に始まる一致は、NORMAL
// のキャレットが着いた後に寄る最後の文字の桁で比べるので、起点が最後の文字の上か行の内容の終わり
// なら起点の行の答えにしない（Vim の searchit の `matchpos.col - (ptr[matchpos.col] == NUL) <
// start_pos.col + extra_col`・extra_col は起点の 1 文字の長さ）。そうしないと `/$` の `n` が同じ
// 行の行末に留まる（#1606）。
[[nodiscard]] std::optional<std::size_t>
first_match_past(std::string_view content, const VimPattern &pattern, std::size_t index)
{
    const auto found = first_match(content, pattern, index + 1);
    const bool on_last =
        vim_character_end(content, Offset{std::min(index, content.size())}).value >= content.size();
    return found.has_value() && found.value() == content.size() && on_last ? std::nullopt : found;
}

// 起点の行の一致。前向きは起点の次の桁以降（first_match_past）、後ろ向きは起点より前。
[[nodiscard]] std::optional<std::size_t> found_in_line(std::string_view content,
                                                       const VimPattern &pattern,
                                                       VimSearchDirection direction,
                                                       std::size_t index)
{
    return direction == VimSearchDirection::forward ? first_match_past(content, pattern, index)
                                                    : last_match(content, pattern, index);
}

// 前向きに、行 begin から行 last までを見る（どちらも行の始まり）。値は当たりのバイト位置。
// 起点でない行は行全体を見るので、行の中の最初の一致がその行の答え。
[[nodiscard]] std::optional<std::size_t>
found_forward(VimSearchWalk &walk, const VimPattern &pattern, std::size_t begin, std::size_t last)
{
    std::optional<std::size_t> line = walk.candidate_after(begin);
    while (line.has_value() && line.value() <= last)
    {
        const std::size_t start = line.value();
        const auto found = first_match(walk.line_at(start), pattern, 0);
        if (found.has_value())
        {
            return start + found.value();
        }
        const auto next = walk.line_after(start);
        line = next.has_value() && start < last ? walk.candidate_after(next.value()) : std::nullopt;
    }
    return std::nullopt;
}

// 後ろ向きに、行 from から行 lowest までを見る。行の中の最後の一致がその行の答え。
[[nodiscard]] std::optional<std::size_t>
found_backward(VimSearchWalk &walk, const VimPattern &pattern, std::size_t from, std::size_t lowest)
{
    std::optional<std::size_t> line = walk.candidate_before(from);
    while (line.has_value() && line.value() >= lowest)
    {
        const std::size_t start = line.value();
        const std::string_view content = walk.line_at(start);
        const auto found = last_match(content, pattern, content.size() + 1);
        if (found.has_value())
        {
            return start + found.value();
        }
        const auto previous = walk.line_before(start);
        line = previous.has_value() && start > lowest ? walk.candidate_before(previous.value())
                                                      : std::nullopt;
    }
    return std::nullopt;
}

using Landing = std::optional<std::pair<Offset, bool>>;

[[nodiscard]] Landing landing(std::optional<std::size_t> found, bool wrapped)
{
    return found.has_value() ? Landing{std::pair{Offset{found.value()}, wrapped}} : std::nullopt;
}

// 起点の行の外。前向きは下の行から本文の末尾まで、折り返して先頭から起点の行（行全体）まで。
// 後ろ向きは上の行から先頭まで、折り返して末尾から起点の行まで（#583・意味は行ごとの走査と同じ）。
// request.wrapscan が偽なら折り返さない（Vim の searchit の `!p_ws` で 1 周目だけ・#1561）。
[[nodiscard]] Landing searched_outside(VimSearchWalk &walk, const VimPattern &pattern,
                                       std::size_t first, const VimMatchRequest &request)
{
    const bool wrapscan = request.wrapscan;
    if (request.direction == VimSearchDirection::forward)
    {
        const auto next = walk.line_after(first);
        const auto below = next.has_value()
                               ? found_forward(walk, pattern, next.value(), walk.last_line())
                               : std::nullopt;
        if (below.has_value() || !wrapscan)
        {
            return landing(below, false);
        }
        return landing(found_forward(walk, pattern, 0, first), true);
    }
    const auto previous = walk.line_before(first);
    const auto above =
        previous.has_value() ? found_backward(walk, pattern, previous.value(), 0) : std::nullopt;
    if (above.has_value() || !wrapscan)
    {
        return landing(above, false);
    }
    return landing(found_backward(walk, pattern, walk.last_line(), first), true);
}

// 1 回ぶんの検索。着いたバイト位置と、本文の端を越えて折り返したか。起点の行では必要な桁を
// 要求する（前向きは起点の次の桁以降、後ろ向きは起点より前）。向きと折り返しは request から読む。
[[nodiscard]] Landing searched_once(const TextBuffer &text, Offset from, const VimPattern &pattern,
                                    const VimMatchRequest &request)
{
    VimSearchWalk walk(text, pattern);
    const VimSearchDirection direction = request.direction;
    const std::size_t first = text.line_start(text.position_of(from).line).value;
    const std::size_t index = from.value - first;
    const auto own = found_in_line(walk.line_at(first), pattern, direction, index);
    return own.has_value() ? landing(first + own.value(), false)
                           : searched_outside(walk, pattern, first, request);
}

// 見つからないときの報せ（Vim の searchit: 'wrapscan' なら E486、無ければ向きの端の E385 / E384）。
[[nodiscard]] VimSearchNoticeKind missed(const VimMatchRequest &request) noexcept
{
    if (request.wrapscan)
    {
        return VimSearchNoticeKind::pattern_not_found;
    }
    return request.direction == VimSearchDirection::forward
               ? VimSearchNoticeKind::hit_bottom_without_match
               : VimSearchNoticeKind::hit_top_without_match;
}
} // namespace

std::vector<OffsetRange> vim_line_matches(std::string_view line, const VimPattern &pattern)
{
    std::vector<OffsetRange> matches;
    visit_line_matches(line, pattern,
                       [&matches](const VimPatternMatch &match)
                       {
                           matches.push_back(OffsetRange{Offset{match.begin}, Offset{match.end}});
                           return true;
                       });
    return matches;
}

std::vector<OffsetRange> vim_window_matches(std::string_view text, OffsetRange window,
                                            const VimPattern &pattern)
{
    std::vector<OffsetRange> matches;
    visit_matches(text, window, pattern,
                  [&matches, window](const VimPatternMatch &match)
                  {
                      if (match.begin >= window.end.value)
                      {
                          return false;
                      }
                      matches.push_back(OffsetRange{Offset{match.begin},
                                                    Offset{std::min(match.end, window.end.value)}});
                      return true;
                  });
    return matches;
}

namespace
{
// 折り返しで最初の当たりへ戻ったら、当たりの列は step 歩ごとの繰り返しなので、残りの回数から
// 繰り返しの分を飛ばす（巨大な回数の `n` `N` `*` `#` が折り返しを回り続けない・#1334）。1 歩の
// 当たりは位置だけで決まり、戻るまでに折り返しは必ず起きている。
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
} // namespace

std::expected<VimSearchHit, VimSearchNoticeKind>
vim_find_match(const TextBuffer &text, const VimPattern &pattern, const VimMatchRequest &request)
{
    Offset at = text.offset_of(request.from);
    bool wrapped = false;
    std::optional<Offset> first = std::nullopt;
    for (std::size_t step = 0; step < request.count; ++step)
    {
        const auto hit = searched_once(text, at, pattern, request);
        if (!hit.has_value())
        {
            return std::unexpected(missed(request));
        }
        at = hit.value().first;
        wrapped = wrapped || hit.value().second;
        step = cycled_step(first, at, step, request.count);
    }
    return VimSearchHit{text.position_of(at), wrapped};
}
} // namespace nenenib::core
