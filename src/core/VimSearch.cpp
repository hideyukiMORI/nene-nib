#include "VimSearch.hpp"

#include "LineNumber.hpp"
#include "Utf8.hpp"
#include "VimPatternMatch.hpp"
#include "VimSearchWalk.hpp"

#include <algorithm>
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

[[nodiscard]] std::optional<std::size_t> found_in_line(std::string_view content,
                                                       const VimPattern &pattern,
                                                       VimSearchDirection direction,
                                                       std::size_t bound)
{
    return direction == VimSearchDirection::forward ? first_match(content, pattern, bound)
                                                    : last_match(content, pattern, bound);
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
[[nodiscard]] Landing searched_outside(VimSearchWalk &walk, const VimPattern &pattern,
                                       std::size_t first, VimSearchDirection direction)
{
    if (direction == VimSearchDirection::forward)
    {
        const auto next = walk.line_after(first);
        const auto below = next.has_value()
                               ? found_forward(walk, pattern, next.value(), walk.last_line())
                               : std::nullopt;
        return below.has_value() ? landing(below, false)
                                 : landing(found_forward(walk, pattern, 0, first), true);
    }
    const auto previous = walk.line_before(first);
    const auto above =
        previous.has_value() ? found_backward(walk, pattern, previous.value(), 0) : std::nullopt;
    return above.has_value()
               ? landing(above, false)
               : landing(found_backward(walk, pattern, walk.last_line(), first), true);
}

// 1 回ぶんの検索。着いたバイト位置と、本文の端を越えて折り返したか。起点の行では必要な桁を
// 要求する（前向きは起点の次の桁以降、後ろ向きは起点より前）。
[[nodiscard]] Landing searched_once(const TextBuffer &text, Offset from, const VimPattern &pattern,
                                    VimSearchDirection direction)
{
    VimSearchWalk walk(text, pattern);
    const std::size_t first = text.line_start(text.position_of(from).line).value;
    const std::size_t index = from.value - first;
    const auto own = found_in_line(walk.line_at(first), pattern, direction,
                                   direction == VimSearchDirection::forward ? index + 1 : index);
    return own.has_value() ? landing(first + own.value(), false)
                           : searched_outside(walk, pattern, first, direction);
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

std::expected<VimSearchHit, VimSearchNoticeKind>
vim_find_match(const TextBuffer &text, const VimPattern &pattern, const VimMatchRequest &request)
{
    Offset at = text.offset_of(request.from);
    bool wrapped = false;
    for (std::size_t step = 0; step < request.count; ++step)
    {
        const auto hit = searched_once(text, at, pattern, request.direction);
        if (!hit.has_value())
        {
            return std::unexpected(VimSearchNoticeKind::pattern_not_found);
        }
        at = hit.value().first;
        wrapped = wrapped || hit.value().second;
    }
    return VimSearchHit{text.position_of(at), wrapped};
}
} // namespace nenenib::core
