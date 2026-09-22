#include "VimSearch.hpp"

#include "LineNumber.hpp"
#include "Utf8.hpp"
#include "VimPatternMatch.hpp"

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

// 行の中で lower 桁以降にある最も左の一致の始まり。
[[nodiscard]] std::optional<std::size_t> first_match(std::string_view content,
                                                     const VimPattern &pattern, std::size_t lower)
{
    for (const auto &match : vim_line_matches(content, pattern))
    {
        if (match.begin.value >= lower)
        {
            return match.begin.value;
        }
    }
    return std::nullopt;
}

// 行の中で upper 桁より前にある最後の一致の始まり。
[[nodiscard]] std::optional<std::size_t> last_match(std::string_view content,
                                                    const VimPattern &pattern, std::size_t upper)
{
    std::optional<std::size_t> found;
    for (const auto &match : vim_line_matches(content, pattern))
    {
        if (match.begin.value >= upper)
        {
            return found;
        }
        found = match.begin.value;
    }
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

// 起点の行では必要な桁を要求し、ほかの行では行全体を見る（前向きは 0 桁目以降、
// 後ろ向きは行の長さより前＝どこでもよい）。
[[nodiscard]] std::size_t bound_of(std::size_t index, std::size_t step, std::size_t size,
                                   VimSearchDirection direction) noexcept
{
    if (direction == VimSearchDirection::forward)
    {
        return step == 0 ? index + 1 : 0;
    }
    return step == 0 ? index : size + 1;
}

[[nodiscard]] std::size_t rotated(std::size_t start, std::size_t step, std::size_t lines,
                                  VimSearchDirection direction) noexcept
{
    const std::size_t turns = step % lines;
    if (direction == VimSearchDirection::forward)
    {
        return (start - 1 + turns) % lines + 1;
    }
    return (start - 1 + lines - turns) % lines + 1;
}

[[nodiscard]] bool wrapped_at(std::size_t start, std::size_t step, std::size_t lines,
                              VimSearchDirection direction) noexcept
{
    if (direction == VimSearchDirection::forward)
    {
        return start - 1 + step >= lines;
    }
    return step >= start;
}

// 1 回ぶんの検索。着いたバイト位置と、本文の端を越えて折り返したか。
[[nodiscard]] std::optional<std::pair<Offset, bool>> searched_once(const TextBuffer &text,
                                                                   Offset from,
                                                                   const VimPattern &pattern,
                                                                   VimSearchDirection direction)
{
    const std::size_t start = text.position_of(from).line.value;
    const std::size_t index = from.value - text.line_start(LineNumber{start}).value;
    const std::size_t lines = text.line_count();
    // 起点の行から 1 行ずつ外へ。最後の段は折り返して起点の行そのものをもう一度見る。
    for (std::size_t step = 0; step <= lines; ++step)
    {
        const LineNumber line{rotated(start, step, lines, direction)};
        const std::string content = text.line_text(line);
        const std::size_t bound = bound_of(index, step, content.size(), direction);
        const auto found = found_in_line(content, pattern, direction, bound);
        if (found.has_value())
        {
            return std::pair{Offset{text.line_start(line).value + found.value()},
                             wrapped_at(start, step, lines, direction)};
        }
    }
    return std::nullopt;
}
} // namespace

std::vector<OffsetRange> vim_line_matches(std::string_view line, const VimPattern &pattern)
{
    std::vector<OffsetRange> matches;
    std::size_t at = 0;
    while (true)
    {
        const auto match = pattern.matched(line, at);
        if (!match.has_value())
        {
            return matches;
        }
        matches.push_back(OffsetRange{Offset{match.value().begin}, Offset{match.value().end}});
        at = advanced(line, match.value());
        if (at >= line.size())
        {
            return matches;
        }
    }
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
